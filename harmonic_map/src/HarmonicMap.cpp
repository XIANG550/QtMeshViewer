#include <math.h>
#include <float.h>

#include <Eigen/Sparse>

#include "HarmonicMap.h"

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279
#endif

void MeshLib::CHarmonicMap::set_mesh(CHarmonicMapMesh* pMesh)
{
    m_pMesh = pMesh; 
    
    // 1. compute the weights of edges
    _calculate_edge_weight();

    // 2. map the boundary to unit circle
    _set_boundary();

    // 3. initialize the map of interior vertices to (0, 0)
    using M = CHarmonicMapMesh;
    for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
    {
        M::CVertex* pV = *viter;
        if (pV->boundary())
            continue;
		//insert your code here
        pV->uv() = CPoint2(0, 0); // move all vertices to center except the boundary verticies

    }
}

double MeshLib::CHarmonicMap::step_one() 
{
    if (!m_pMesh)
    {
        std::cerr << "Should set mesh first!" << std::endl;
        return DBL_MAX;
    }

    using M = CHarmonicMapMesh;

    // move each interior vertex to its weighted center of neighbors
    double max_error = -DBL_MAX;
    for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
    {
        M::CVertex* pV = *viter;
        if (pV->boundary()) 
            continue;

        double sw = 0;
        CPoint2 suv(0, 0);
        for (M::VertexVertexIterator vviter(pV); !vviter.end(); vviter++)
        {
            M::CVertex* pW = *vviter;
            M::CEdge* pE = m_pMesh->vertexEdge(pV, pW);

			//insert your code here ->Considering each interior vertex and sum the w1j up for j its neighbor vetex
            sw += pE->weight();
            suv += pW->uv() * pE->weight(); 
        }
        suv /= sw;

        double error = (pV->uv() - suv).norm();
        max_error = (error > max_error) ? error : max_error;
        pV->uv() = suv;
    }

    printf("Current max error is %g\n", max_error);
    return max_error;
}

void MeshLib::CHarmonicMap::iterative_map(double epsilon)
{
    if (!m_pMesh)
    {
        std::cerr << "Should set mesh first!" << std::endl;
        return;
    }

    using M = CHarmonicMapMesh;

    // take steps until it converges.
    while (true)
    {
        double error = this->step_one();
        if (error < epsilon)
            break;
    }
}

void MeshLib::CHarmonicMap::map() 
{
    if (!m_pMesh)
    {
        std::cerr << "Should set mesh first!" << std::endl;
        return;
    }

    using M = CHarmonicMapMesh;

    // 1. Initialize
    int vid = 0;  // interior vertex id
    int bid = 0;  // boundary vertex id
    for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
    {
        M::CVertex* pV = *viter;

        // id starts from 1
        if (pV->boundary())
            pV->idx() = bid++;
        else
            pV->idx() = vid++;
    }

    int interior_vertices = vid;
    int boundary_vertices = bid;

    // 2. Set the matrix A and B
    std::vector<Eigen::Triplet<double>> A_coefficients; // for interior
    std::vector<Eigen::Triplet<double>> B_coefficients; // for boundary

    for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
    {
        // pV does not include the boundary vertex
        M::CVertex* pV = *viter;
        if (pV->boundary())
            continue;

        int vid = pV->idx();

        double sw = 0;
        for (M::VertexVertexIterator witer(pV); !witer.end(); ++witer)
        {
            M::CVertex* pW = *witer;
            int wid = pW->idx();

            // Each edge around vertex pV and corresponding weight
            M::CEdge* e = m_pMesh->vertexEdge(pV, pW);
            double w = e->weight();

			//insert your code here
			//construct one element of the matrix A and B, using 
			//Eigen::Triplet<double>(i,j, val)
			//there push_back the triplet to A or B coefficients
           
            if (!pW->boundary())
            {
                A_coefficients.push_back(Eigen::Triplet<double>(vid, wid, -w)); // - to flip the sign back to ij
            }
            else 
            {
                B_coefficients.push_back(Eigen::Triplet<double>(vid, wid, w));
            }
             sw += w;
        }
		//insert the diagonal element
        A_coefficients.push_back(Eigen::Triplet<double>(vid, vid, sw));
    }

    // creating sparse matrix of given size
    Eigen::SparseMatrix<double> A(interior_vertices, interior_vertices);
    A.setZero();
    Eigen::SparseMatrix<double> B(interior_vertices, boundary_vertices);
    B.setZero();

    // filling in the coefficients 11, 12, 13, ... , ij
    A.setFromTriplets(A_coefficients.begin(), A_coefficients.end());
    B.setFromTriplets(B_coefficients.begin(), B_coefficients.end());

    // 3. Solve the equations
    Eigen::ConjugateGradient<Eigen::SparseMatrix<double>> solver;
    std::cerr << "Eigen Decomposition" << std::endl;
    solver.compute(A);
    std::cerr << "Eigen Decomposition Finished" << std::endl;

    if (solver.info() != Eigen::Success)
    {
        std::cerr << "Warning: Eigen decomposition failed" << std::endl;
    }

    for (int k = 0; k < 2; k++)
    {
        Eigen::VectorXd b(boundary_vertices);
        // set boundary constraints vector b
        for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
        {
            M::CVertex* pV = *viter;
            if (!pV->boundary())
                continue;
            int id = pV->idx();
            b(id) = pV->uv()[k];
        }

        Eigen::VectorXd c(interior_vertices);
        c = B * b;

        Eigen::VectorXd x = solver.solve(c); // Ax=c
        if (solver.info() != Eigen::Success)
        {
            std::cerr << "Waring: Eigen decomposition failed" << std::endl;
        }

        // set the images of the harmonic map to interior vertices
        for (M::MeshVertexIterator viter(m_pMesh); !viter.end(); ++viter)
        {
            M::CVertex* pV = *viter;
            if (pV->boundary())
                continue;
            int id = pV->idx();
            pV->uv()[k] = x(id);
        }
    }
}

void MeshLib::CHarmonicMap::_calculate_edge_weight() 
{
    using M = CHarmonicMapMesh;

    // 1. compute edge length
    for (M::MeshEdgeIterator eiter(m_pMesh); !eiter.end(); ++eiter)
    {
        M::CEdge* pE = *eiter;
        M::CVertex* v1 = m_pMesh->edgeVertex1(pE);
        M::CVertex* v2 = m_pMesh->edgeVertex2(pE);
        pE->length() = (v1->point() - v2->point()).norm();
    }

    // 2. compute corner angle
    for (M::MeshFaceIterator fiter(m_pMesh); !fiter.end(); ++fiter)
    {
        M::CFace* pF = *fiter;
        M::CHalfEdge* pH[3];
        //insert your code here
        pH[0] = m_pMesh->faceHalfedge(pF);
        pH[1] = m_pMesh->halfedgeNext(pH[0]);
        pH[2] = m_pMesh->halfedgeNext(pH[1]);

        double len[3];
        for (int i = 0; i < 3; i++)
            len[i] = m_pMesh->halfedgeEdge(pH[i])->length();

        // use inverse cosine law to compute the corner angles "https://en.wikipedia.org/wiki/Law_of_cosines"
        double c = len[0], a = len[1], b = len[2];
        pH[0]->angle() = _inverse_cosine_law(a, b, c);
        pH[1]->angle() = _inverse_cosine_law(b, c, a);
        pH[2]->angle() = _inverse_cosine_law(c, a, b);

    }

    // 3. compute edge weight
    for (M::MeshEdgeIterator eiter(m_pMesh); !eiter.end(); ++eiter)
    {
        M::CEdge* pE = *eiter;
		//insert your code here
		//set cotangent edge weight
        if (!pE->boundary())
        {
            double theta[2];
            theta[0] = m_pMesh->halfedgeNext(m_pMesh->edgeHalfedge(pE, 0))->angle(); // interior 0
            theta[1] = m_pMesh->halfedgeNext(m_pMesh->edgeHalfedge(pE, 1))->angle(); // dual     1
            pE->weight() = std::cos(theta[0]) / std::sin(theta[0]) + std::cos(theta[1]) / std::sin(theta[1]);
        }
        else
        {
            double theta = m_pMesh->halfedgeNext(m_pMesh->edgeHalfedge(pE, 0))->angle(); 
            pE->weight() = std::cos(theta) / std::sin(theta);
        }

    }
}

void MeshLib::CHarmonicMap::_set_boundary() 
{
    using M = CHarmonicMapMesh;

    // 1. get the boundary half edge loop
    M::CBoundary boundary(m_pMesh);
    std::vector<M::CLoop*>& pLs = boundary.loops();
    if (pLs.size() != 1)
    {
        std::cerr << "Only topological disk accepted!" << std::endl;
        exit(EXIT_FAILURE);
    }
    M::CLoop* pL = pLs[0];
    std::list<M::CHalfEdge*>& pHs = pL->halfedges();
    
    // 2. compute the total length of the boundary
    double sum = 0.0;
    std::list<M::CHalfEdge*>::iterator it;
    for (it = pHs.begin(); it != pHs.end(); ++it)
    {
        M::CHalfEdge* pH = *it;
        sum += m_pMesh->halfedgeEdge(pH)->length();
    }

    // 3. parameterize the boundary using arc length parameter
    double len = 0.0;
    for (it = pHs.begin(); it != pHs.end(); ++it)
    {
        M::CHalfEdge* pH = *it;
        M::CVertex* pV = m_pMesh->halfedgeVertex(pH);

        len += m_pMesh->halfedgeEdge(pH)->length();
        double angle = len / sum * 2.0 * M_PI;
        pV->uv() = CPoint2(cos(angle), sin(angle)); 
    }
}

double MeshLib::CHarmonicMap::_inverse_cosine_law(double a, double b, double c) 
{ 
    double cs = (a * a + b * b - c * c) / (2.0 * a * b);
    assert(cs <= 1.0 && cs >= -1.0);
    return std::acos(cs);
}

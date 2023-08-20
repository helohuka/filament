#ifndef __ICOSPHERE_H__
#define __ICOSPHERE_H__ 1

/*!
 * \file IcoSphere.h
 * \date 2023.08.20
 *
 * \author Helohuka
 * 
 * Contact: helohuka@outlook.com
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note
*/

#include <math/vec3.h>

#include <map>
#include <vector>

class IcoSphere
{
public:
    using Index = uint16_t;

    struct Triangle
    {
        Index vertex[3];
    };

    using TriangleList = std::vector<Triangle>;
    using VertexList   = std::vector<filament::math::float3>;
    using IndexedMesh  = std::pair<VertexList, TriangleList>;

    explicit IcoSphere(size_t subdivisions);

    IndexedMesh const&  getMesh() const { return mMesh; }
    VertexList const&   getVertices() const { return mMesh.first; }
    TriangleList const& getIndices() const { return mMesh.second; }

private:
    static const IcoSphere::VertexList   sVertices;
    static const IcoSphere::TriangleList sTriangles;

    using Lookup = std::map<std::pair<Index, Index>, Index>;
    Index        vertex_for_edge(Lookup& lookup, VertexList& vertices, Index first, Index second);
    TriangleList subdivide(VertexList& vertices, TriangleList const& triangles);
    IndexedMesh  make_icosphere(int subdivisions);

    IndexedMesh mMesh;
};

#endif // __ICOSPHERE_H__

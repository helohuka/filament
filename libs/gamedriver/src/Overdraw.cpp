#include "gamedriver/Overdraw.h"

OverdrawVisualizer::OverdrawVisualizer(filament::Engine& engine, filament::Scene* scene, filament::Material const* material) :
    mRenderEngine(engine)
{
    static constexpr filament::math::float4 sFullScreenTriangleVertices[3] = {
        {-1.0f, -1.0f, 1.0f, 1.0f},
        {3.0f, -1.0f, 1.0f, 1.0f},
        {-1.0f, 3.0f, 1.0f, 1.0f}};

    static const uint16_t sFullScreenTriangleIndices[3] = {0, 1, 2};

    const filament::math::float3 overdrawColors[OVERDRAW_LAYERS] = {
        {0.0f, 0.0f, 1.0f}, // blue         (overdrawn 1 time)
        {0.0f, 1.0f, 0.0f}, // green        (overdrawn 2 times)
        {1.0f, 0.0f, 1.0f}, // magenta      (overdrawn 3 times)
        {1.0f, 0.0f, 0.0f}  // red          (overdrawn 4+ times)
    };

    for (auto i = 0; i < OVERDRAW_LAYERS; i++)
    {
        mMIArray[i] = material->createInstance();
        // TODO: move this to the material definition.
        mMIArray[i]->setStencilCompareFunction(filament::MaterialInstance::StencilCompareFunc::E);
        // The stencil value represents the number of times the fragment has been written to.
        // We want 0-1 writes to be the regular color. Overdraw visualization starts at 2+ writes,
        // which represents a fragment overdrawn 1 time.
        mMIArray[i]->setStencilReferenceValue(i + 2);
        mMIArray[i]->setParameter("color", overdrawColors[i]);
    }
    auto& lastMi = mMIArray[OVERDRAW_LAYERS - 1];
    // This seems backwards, but it isn't. The comparison function compares:
    // the reference value (left side) <= stored stencil value (right side)
    lastMi->setStencilCompareFunction(filament::MaterialInstance::StencilCompareFunc::LE);

    mVertexBuffer = filament::VertexBuffer::Builder()
                                     .vertexCount(3)
                                     .bufferCount(1)
                                               .attribute(filament::VertexAttribute::POSITION, 0, filament::VertexBuffer::AttributeType::FLOAT4, 0)
                                     .build(mRenderEngine);

    mVertexBuffer->setBufferAt(
        mRenderEngine, 0, {sFullScreenTriangleVertices, sizeof(sFullScreenTriangleVertices)});

    mIndexBuffer = filament::IndexBuffer::Builder()
                                   .indexCount(3)
                                             .bufferType(filament::IndexBuffer::IndexType::USHORT)
                                   .build(mRenderEngine);

    mIndexBuffer->setBuffer(mRenderEngine,
                           {sFullScreenTriangleIndices, sizeof(sFullScreenTriangleIndices)});

    auto&       em           = utils::EntityManager::get();
    
    for (auto i = 0; i < OVERDRAW_LAYERS; i++)
    {
        mRenderableArray[i] = em.create();
        filament::RenderableManager::Builder(1)
            .boundingBox({{}, {1.0f, 1.0f, 1.0f}})
            .material(0, mMIArray[i])
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES, mVertexBuffer, mIndexBuffer, 0, 3)
            .culling(false)
            .priority(7u) // ensure the overdraw primitives are drawn last
            .layerMask(0xFF, 1u << OVERDRAW_VISIBILITY_LAYER)
            .build(mRenderEngine, mRenderableArray[i]);
        scene->addEntity(mRenderableArray[i]);
    }

}

OverdrawVisualizer::~OverdrawVisualizer()
{
    auto& em = utils::EntityManager::get();
    for (auto e : mRenderableArray)
    {
        mRenderEngine.destroy(e);
        em.destroy(e);
    }

    for (auto mi : mMIArray)
    {
        mRenderEngine.destroy(mi);
    }

}
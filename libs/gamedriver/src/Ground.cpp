#include "gamedriver/Ground.h"
#include "gamedriver/Configure.h"

Ground::Ground(filament::Engine& engine, filament::Scene* scene, filament::Material const* material) :
    mRenderEngine(engine)
{
    auto&     em             = utils::EntityManager::get();
    
    mMaterialInstance = material->createInstance();

    auto& viewerOptions = gSettings.viewer;
    mMaterialInstance->setParameter("strength", viewerOptions.groundShadowStrength);

    const static uint32_t indices[] = {
        0, 1, 2, 2, 3, 0};

    /*filament::Aabb        aabb;
    filament::math::mat4f transform = fitIntoUnitCube(aabb, 4);
    aabb                            = aabb.transform(transform);*/
    

    filament::math::float3 planeExtent{100.0f , 0.0f, 100.0f };

    const static filament::math::float3 vertices[] = {
        {-planeExtent.x, 0, -planeExtent.z},
        {-planeExtent.x, 0, planeExtent.z},
        {planeExtent.x, 0, planeExtent.z},
        {planeExtent.x, 0, -planeExtent.z},
    };

    filament::math::short4 tbn = filament::math::packSnorm16(
        filament::math::mat3f::packTangentFrame(
            filament::math::mat3f{
                filament::math::float3{1.0f, 0.0f, 0.0f},
                filament::math::float3{0.0f, 0.0f, 1.0f},
                filament::math::float3{0.0f, 1.0f, 0.0f}})
            .xyzw);

    const static filament::math::short4 normals[]{tbn, tbn, tbn, tbn};

    mVertexBuffer = filament::VertexBuffer::Builder()
                                     .vertexCount(4)
                                     .bufferCount(2)
                                               .attribute(filament::VertexAttribute::POSITION,
                                                          0, filament::VertexBuffer::AttributeType::FLOAT3)
                                               .attribute(filament::VertexAttribute::TANGENTS,
                                                          1, filament::VertexBuffer::AttributeType::SHORT4)
                                               .normalized(filament::VertexAttribute::TANGENTS)
                                     .build(mRenderEngine);

    mVertexBuffer->setBufferAt(mRenderEngine, 0, filament::VertexBuffer::BufferDescriptor(vertices, mVertexBuffer->getVertexCount() * sizeof(vertices[0])));
    mVertexBuffer->setBufferAt(mRenderEngine, 1, filament::VertexBuffer::BufferDescriptor(normals, mVertexBuffer->getVertexCount() * sizeof(normals[0])));

    mIndexBuffer = filament::IndexBuffer::Builder()
                                   .indexCount(6)
                                   .build(mRenderEngine);

    mIndexBuffer->setBuffer(mRenderEngine, filament::IndexBuffer::BufferDescriptor(indices, mIndexBuffer->getIndexCount() * sizeof(uint32_t)));

    mRenderable = em.create();
    filament::RenderableManager::Builder(1)
        .boundingBox({{}, {planeExtent.x, 1e-4f, planeExtent.z}})
        .material(0, mMaterialInstance)
        .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                  mVertexBuffer, mIndexBuffer, 0, 6)
        .culling(false)
        .receiveShadows(true)
        .castShadows(false)
        .build(mRenderEngine, mRenderable);

    
    auto& tcm = mRenderEngine.getTransformManager();
    tcm.setTransform(tcm.getInstance(mRenderable),
                     filament::mat4f::translation(filament::float3{0, 0, 0}));

    auto& rcm      = mRenderEngine.getRenderableManager();
    auto  instance = rcm.getInstance(mRenderable);
    rcm.setLayerMask(instance, 0xff, 0x00);

    scene->addEntity(mRenderable);
}

Ground::~Ground()
{
    auto& em = utils::EntityManager::get();

    mRenderEngine.destroy(mRenderable);
    em.destroy(mRenderable);
    mRenderEngine.destroy(mVertexBuffer);
    mRenderEngine.destroy(mIndexBuffer);
    mRenderEngine.destroy(mMaterialInstance);
}
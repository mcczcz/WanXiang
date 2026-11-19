#pragma once

#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/deps/vanilla_components/block_source_visitor/CollisionShape.h"
#include "mc/world/level/dimension/DimensionType.h"
#include <algorithm>
#include <vector>

namespace my_mod {

// doTick only borrows this adapter for the duration of the call. All world
// queries still use the original (possibly thread-local) block source.
class PistonCollisionBlockSource final : public ::IConstBlockSource {
    using Entity = ::optional_ref<::GetCollisionShapeInterface const>;

    ::IConstBlockSource const& mSource;
    bool (*mIsImmune)(Entity);
    bool (*mIsPiston)(::Block const*);

public:
    PistonCollisionBlockSource(
        ::IConstBlockSource const& source,
        bool (*isImmune)(Entity),
        bool (*isPiston)(::Block const*)
    )
    : mSource(source),
      mIsImmune(isImmune),
      mIsPiston(isPiston) {}

    ::Block const& getBlock(::BlockPos const& pos) const override { return mSource.getBlock(pos); }
    ::Block const& getBlock(::BlockPos const& pos, uint layer) const override { return mSource.getBlock(pos, layer); }
    ::BlockActor const* getBlockEntity(::BlockPos const& pos) const override { return mSource.getBlockEntity(pos); }
    ::Block const&      getExtraBlock(::BlockPos const& pos) const override { return mSource.getExtraBlock(pos); }
    ::Block const&      getLiquidBlock(::BlockPos const& pos) const override { return mSource.getLiquidBlock(pos); }
    bool                hasBlock(::BlockPos const& pos) const override { return mSource.hasBlock(pos); }
    bool                containsAnyLiquid(::AABB const& box) const override { return mSource.containsAnyLiquid(box); }
    bool containsMaterial(::AABB const& box, ::SharedTypes::v1_26_20::MaterialType material) const override {
        return mSource.containsMaterial(box, material);
    }
    bool isInWall(::Vec3 const& pos) const override { return mSource.isInWall(pos); }
    bool isUnderWater(::BlockPos const& pos, ::Block const& block) const override {
        return mSource.isUnderWater(pos, block);
    }
    ::Material const& getMaterial(::BlockPos const& pos) const override { return mSource.getMaterial(pos); }
    ::Material const& getMaterial(int x, int y, int z) const override { return mSource.getMaterial(x, y, z); }
    bool              hasBorderBlock(::BlockPos const pos) const override { return mSource.hasBorderBlock(pos); }
    bool              hasChunk(::ChunkPos const& pos, bool serverSideOnly) const override {
        return mSource.hasChunk(pos, serverSideOnly);
    }
    bool hasChunksAt(::Bounds const& bounds, bool ignoreClientChunk) const override {
        return mSource.hasChunksAt(bounds, ignoreClientChunk);
    }
    bool hasChunksAt(::BlockPos const& pos, int r, bool ignoreClientChunk) const override {
        return mSource.hasChunksAt(pos, r, ignoreClientChunk);
    }
    bool hasChunksAt(::AABB const& box, bool ignoreClientChunk) const override {
        return mSource.hasChunksAt(box, ignoreClientChunk);
    }
    ::DimensionType getDimensionId() const override { return mSource.getDimensionId(); }
    bool shouldFireEvents(::LevelChunk const& chunk) const override { return mSource.shouldFireEvents(chunk); }
    void fetchAABBs(::std::vector<::AABB>& shapes, ::AABB const& box, bool unloaded) const override {
        mSource.fetchAABBs(shapes, box, unloaded);
    }

    void fetchCollisionShapes(
        ::std::vector<::AABB>& shapes,
        ::AABB const&          box,
        bool                   unloaded,
        Entity                 entity,
        ::std::vector<::AABB>* tempShapes
    ) const override {
        if (!mIsImmune(entity)) {
            mSource.fetchCollisionShapes(shapes, box, unloaded, entity, tempShapes);
            return;
        }
        // Keep block identity until filtering is complete; AABBs alone cannot
        // distinguish a piston head from a neighbouring ordinary block.
        ::std::vector<::BlockSourceVisitor::CollisionShape> identified;
        mSource.fetchCollisionShapesAndBlocks(identified, box, unloaded, entity, tempShapes);
        for (auto const& shape : identified) {
            ::Block const* block = shape.mMaybeBlock;
            if (block == nullptr || !mIsPiston(block)) {
                shapes.push_back(*shape.mAABB);
            }
        }
    }

    void fetchCollisionShapesAndBlocks(
        ::std::vector<::BlockSourceVisitor::CollisionShape>& shapes,
        ::AABB const&                                        box,
        bool                                                 unloaded,
        Entity                                               entity,
        ::std::vector<::AABB>*                               tempShapes
    ) const override {
        auto const firstNewShape = shapes.size();
        mSource.fetchCollisionShapesAndBlocks(shapes, box, unloaded, entity, tempShapes);
        if (mIsImmune(entity)) {
            shapes.erase(
                std::remove_if(
                    shapes.begin() + firstNewShape,
                    shapes.end(),
                    [this](auto const& shape) {
                        ::Block const* block = shape.mMaybeBlock;
                        return block != nullptr && mIsPiston(block);
                    }
                ),
                shapes.end()
            );
        }
    }

    ::AABB getTallestCollisionShape(::AABB const& box, float* offset, bool unloaded, Entity entity) const override {
        return mSource.getTallestCollisionShape(box, offset, unloaded, entity);
    }
    float getBrightness(::BlockPos const& pos) const override { return mSource.getBrightness(pos); }
};

} // namespace my_mod

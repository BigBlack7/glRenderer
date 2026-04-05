#include "renderStateCache.hpp"

namespace core
{
    RenderStateCache::RenderStateCache()
    {
        Reset();
    }

    void RenderStateCache::Reset()
    {
        mProgram = 0;
        mVao = 0;
        mTexture2D.fill(0); // 重置所有纹理单元状态
        mTexture2DArray.fill(0);
        mTextureCube.fill(0);
    }

    bool RenderStateCache::UseProgram(GLuint program)
    {
        // 缓存检查: 如果程序未变更, 直接跳过
        if (mProgram == program)
            return false;

        glUseProgram(program);
        mProgram = program;
        return true;
    }

    bool RenderStateCache::BindVertexArray(GLuint vao)
    {
        if (mVao == vao)
            return false;

        glBindVertexArray(vao);
        mVao = vao;
        return true;
    }

    bool RenderStateCache::BindTexture2D(uint32_t unit, GLuint textureID)
    {
        // 确保纹理单元在有效范围内
        if (unit >= MaxTextureUnits)
            return false;

        // 该纹理单元是否已经绑定了目标纹理
        if (mTexture2D[unit] == textureID)
            return false;

        glBindTextureUnit(unit, textureID);
        mTexture2D[unit] = textureID; // 更新缓存状态
        return true;
    }

    bool RenderStateCache::BindTexture2DArray(uint32_t unit, GLuint textureID)
    {
        if (unit >= MaxTextureUnits)
            return false;

        if (mTexture2DArray[unit] == textureID)
            return false;

        glBindTextureUnit(unit, textureID);
        mTexture2DArray[unit] = textureID;
        return true;
    }

    bool RenderStateCache::BindTextureCube(uint32_t unit, GLuint textureID)
    {
        if (unit >= MaxTextureUnits)
            return false;

        if (mTextureCube[unit] == textureID)
            return false;

        glBindTextureUnit(unit, textureID);
        mTextureCube[unit] = textureID;
        return true;
    }
}
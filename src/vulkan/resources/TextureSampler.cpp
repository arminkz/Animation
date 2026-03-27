#include "TextureSampler.h"


TextureSampler::TextureSampler(std::shared_ptr<VulkanContext> ctx, uint32_t mipLevels, VkSamplerAddressMode addressMode)
    : _ctx(std::move(ctx))
{
    // Retrieve the physical device properties for the texture sampler
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_ctx->physicalDevice, &properties);

    // Create texture sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if(vkCreateSampler(_ctx->device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS) {
        spdlog::error("Failed to create texture sampler!");
    }
}

TextureSampler::~TextureSampler()
{
    vkDestroySampler(_ctx->device, _textureSampler, nullptr);
}
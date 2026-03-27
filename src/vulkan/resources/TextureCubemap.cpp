#include "TextureCubemap.h"

#include "stb_image.h"

TextureCubemap::TextureCubemap(std::shared_ptr<VulkanContext> ctx, const std::string& path, VkFormat format)
    : _ctx(std::move(ctx))
{
    //cubemap images
    std::vector<std::string> faces = {
        path + "/px.png",
        path + "/nx.png",
        path + "/py.png",
        path + "/ny.png",
        path + "/pz.png",
        path + "/nz.png"
    };

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels[6];

    for (size_t i = 0; i < faces.size(); ++i) {
        pixels[i] = stbi_load(faces[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels[i]) {
            spdlog::error("Failed to load cubemap texture image! {}", faces[i]);
            return;
        }
    }

    int imageSize = texWidth * texHeight * 4;
    int totalSize = imageSize * 6;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx, totalSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, stagingBuffer, stagingBufferMemory);


    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, totalSize, 0, &data);
    for (size_t i = 0; i < 6; ++i) {
        memcpy(static_cast<char*>(data) + imageSize * i, pixels[i], static_cast<size_t>(imageSize));
        stbi_image_free(pixels[i]);
    }
    vkUnmapMemory(_ctx->device, stagingBufferMemory);


    // Create Image
    VulkanHelper::createImage(_ctx, texWidth, texHeight,
        format,
        1,
        6,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _cubemapImage, _cubemapImageMemory,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    // Transition image layout to transfer destination optimal
    VkImageSubresourceRange cubemapRange{};
    cubemapRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cubemapRange.baseMipLevel = 0;
    cubemapRange.levelCount = 1;
    cubemapRange.baseArrayLayer = 0;
    cubemapRange.layerCount = 6;
    VulkanHelper::transitionImageLayout(_ctx, _cubemapImage, cubemapRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy from staging buffer to the cubemap image
    VulkanHelper::copyBufferToCubemap(_ctx, stagingBuffer, _cubemapImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // Staging buffer is no longer needed, so we can destroy it
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    // Transition the image layout to shader read only optimal
    VulkanHelper::transitionImageLayout(_ctx, _cubemapImage, cubemapRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Create ImageView
    _cubemapImageView = VulkanHelper::createImageView(_ctx, _cubemapImage, format, 1, 6, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE);

    // Create texture sampler
    // Retrieve the physical device properties for the texture sampler
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_ctx->physicalDevice, &properties);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(_ctx->device, &samplerInfo, nullptr, &_cubemapSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    spdlog::info("Cubemap texture loaded successfully.");

}

TextureCubemap::~TextureCubemap()
{
    vkDestroySampler(_ctx->device, _cubemapSampler, nullptr);
    vkDestroyImageView(_ctx->device, _cubemapImageView, nullptr);
    vkDestroyImage(_ctx->device, _cubemapImage, nullptr);
    vkFreeMemory(_ctx->device, _cubemapImageMemory, nullptr);
}


VkDescriptorImageInfo TextureCubemap::getDescriptorInfo() const
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = _cubemapSampler;
    imageInfo.imageView = _cubemapImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    return imageInfo;
}
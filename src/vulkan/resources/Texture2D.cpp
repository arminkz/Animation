#include "Texture2D.h"

#include "vulkan/VulkanHelper.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture2D::Texture2D(std::shared_ptr<VulkanContext> ctx, const std::string& path, VkFormat format) 
    : _ctx(std::move(ctx))
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        spdlog::error("Failed to load texture image!");
        return;
    }
    //spdlog::info("Loaded texture image: {} ({}x{}) channels: {}", path, texWidth, texHeight, texChannels);

    _width = texWidth;
    _height = texHeight;
    _mipLevels = glm::min(static_cast<int>(floor(log2(std::max(texWidth, texHeight)))) + 1, 12); //TODO: fix this hardcode!
    _format = format;

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, stagingBuffer, stagingBufferMemory);

    // Copy pixel data to buffer
    void* data;
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, imageSize, 0, &data); // Map the buffer memory into CPU addressable space
    memcpy(data, pixels, (size_t)imageSize); // Copy the pixel data to the mapped memory
    vkUnmapMemory(_ctx->device, stagingBufferMemory); // Unmap the memory

    // Free the loaded image data (from CPU RAM)
    stbi_image_free(pixels); 

    // Create Image
    VulkanHelper::createImage(_ctx, texWidth, texHeight,
        _format,
        _mipLevels,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _textureImage, _textureImageMemory);

    // Transition image layout to transfer destination optimal
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = static_cast<uint32_t>(_mipLevels);
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    VulkanHelper::transitionImageLayout(_ctx, _textureImage, subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy from staging buffer to the image
    VulkanHelper::copyBufferToImage(_ctx, stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // Staging buffer is no longer needed, so we can destroy it
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    // Generate Mipmaps for the texture image
    //spdlog::info("Generating Mipmaps for {} levels: {}", path, _mipLevels);
    generateMipmaps(); // Generate mipmaps for the texture image

    // Create ImageView
    _textureImageView = VulkanHelper::createImageView(_ctx, _textureImage, _format, _mipLevels, 1, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create texture sampler
    // Retrieve the physical device properties for the texture sampler
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_ctx->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(_mipLevels);

    if(vkCreateSampler(_ctx->device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS) {
        spdlog::error("Failed to create texture sampler!");
    }

}

Texture2D::Texture2D(std::shared_ptr<VulkanContext> ctx, const void* pixelData, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels)
    : _ctx(std::move(ctx)), _width(width), _height(height), _format(format)
{
    if (mipLevels == 0) {
        _mipLevels = glm::max(glm::min(static_cast<int>(floor(log2(std::max(width, height)))) + 1, 12), 1);
    } else {
        _mipLevels = mipLevels;
    }

    VkDeviceSize imageSize = _width * _height * 4;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixelData, static_cast<size_t>(imageSize));
    vkUnmapMemory(_ctx->device, stagingBufferMemory);

    // Create Image
    VulkanHelper::createImage(_ctx, _width, _height,
        _format,
        _mipLevels,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _textureImage, _textureImageMemory);

    // Transition image layout to transfer destination optimal
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = static_cast<uint32_t>(_mipLevels);
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    VulkanHelper::transitionImageLayout(_ctx, _textureImage, subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy from staging buffer to the image
    VulkanHelper::copyBufferToImage(_ctx, stagingBuffer, _textureImage, static_cast<uint32_t>(_width), static_cast<uint32_t>(_height));

    // Staging buffer is no longer needed, so we can destroy it
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    generateMipmaps();

    // Create ImageView
    _textureImageView = VulkanHelper::createImageView(_ctx, _textureImage, _format, _mipLevels, 1, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create texture sampler
    // Retrieve the physical device properties for the texture sampler
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_ctx->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

Texture2D::~Texture2D()
{
    cleanup();
}

void Texture2D::cleanup() {
    vkDestroySampler(_ctx->device, _textureSampler, nullptr);
    vkDestroyImageView(_ctx->device, _textureImageView, nullptr);
    vkDestroyImage(_ctx->device, _textureImage, nullptr);
    vkFreeMemory(_ctx->device, _textureImageMemory, nullptr);
}

void Texture2D::generateMipmaps() {
    // Generate mipmaps for the texture image on the GPU
    // In serious applications, mipmaps are precomputed and stored in the texture image
    
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_ctx->physicalDevice, _format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        spdlog::error("Texture image format does not support linear blitting!");
        return;
    }
    
    VkCommandBuffer commandBuffer = VulkanHelper::beginSingleTimeCommands(_ctx); // Begin command buffer recording

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = _textureImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = _width;
    int32_t mipHeight = _height;

    for (uint32_t i = 1; i < _mipLevels; i++) {

        barrier.subresourceRange.baseMipLevel = i - 1; // Previous mip level
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        // Wait for the previous mip level to be ready before transitioning to the next mip level
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        // Blit means bulk copy of pixels from one image to another
        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            _textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            _textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        // Transition the image layout to shader read only optimal after blitting
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        // Update the mip width and height for the next mip level
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition the image layout to shader read only optimal for the last mip level
    barrier.subresourceRange.baseMipLevel = _mipLevels - 1; // Last mip level
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VulkanHelper::endSingleTimeCommands(_ctx, commandBuffer); // End command buffer recording
}

VkDescriptorImageInfo Texture2D::getDescriptorInfo() const
{
    VkDescriptorImageInfo textureInfo{};
    textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textureInfo.imageView = _textureImageView;
    textureInfo.sampler = _textureSampler;
    return textureInfo;
}

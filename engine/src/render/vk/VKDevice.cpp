// AI生成
#include "VKDevice.h"
#include "ui_shaders.h"
#include "../ThreadScheduler.h"
#include "../../core/Log.h"

#include <cstring>
#include <cstdlib>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace ui {

#if UI_VK_ON
static uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t typeFilter,
                               VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}
#endif // UI_VK_ON

VKDevice::VKDevice() = default;
VKDevice::~VKDevice() { shutdown(); }

bool VKDevice::fullyCapable() const {
#if UI_VK_ON
    return true;
#else
    return false;
#endif
}

#if UI_VK_ON

static VKAPI_ATTR VkBool32 VKAPI_CALL dbg(
    VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    UI_LOG_WARN("vk: %s", data->pMessage);
    return VK_FALSE;
}

bool VKDevice::init(void* nativeHandle, int w, int h) {
    native_ = nativeHandle;
    extent_.width = w; extent_.height = h;

    VkApplicationInfo app{}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "uiengine"; app.apiVersion = VK_API_VERSION_1_0;

    uint32_t gc = 0; const char** exts = glfwGetRequiredInstanceExtensions(&gc);
    std::vector<const char*> layers;
#ifndef NDEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    std::vector<const char*> iexts(exts, exts + gc);
#ifndef NDEBUG
    iexts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo ici{}; ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledLayerCount = (uint32_t)layers.size();
    ici.ppEnabledLayerNames = layers.data();
    ici.enabledExtensionCount = (uint32_t)iexts.size();
    ici.ppEnabledExtensionNames = iexts.data();

    if (vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS) {
        UI_LOG_ERROR("vkCreateInstance failed");
        return false;
    }

    if (glfwCreateWindowSurface(instance_, (GLFWwindow*)native_, nullptr, &surface_) != VK_SUCCESS) {
        UI_LOG_ERROR("glfwCreateWindowSurface failed");
        return false;
    }

    uint32_t n = 0; vkEnumeratePhysicalDevices(instance_, &n, nullptr);
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(instance_, &n, devs.data());
    if (devs.empty()) { UI_LOG_ERROR("no vulkan device"); return false; }
    physical_ = devs[0];

    uint32_t qfc = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qfc, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(qfc);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qfc, qfp.data());
    queueFamily_ = 0;
    bool found = false;
    for (uint32_t i = 0; i < qfc; ++i) {
        VkBool32 sup = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_, i, surface_, &sup);
        if (sup && (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { queueFamily_ = i; found = true; break; }
    }
    if (!found) { UI_LOG_ERROR("no graphics queue with surface support"); return false; }

    float prio = 1.f;
    VkDeviceQueueCreateInfo qci{}; qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queueFamily_; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    const char* dexts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{}; dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = dexts;
    if (vkCreateDevice(physical_, &dci, nullptr, &device_) != VK_SUCCESS) {
        UI_LOG_ERROR("vkCreateDevice failed"); return false;
    }
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);

    if (!recreate()) return false;

    VkCommandPoolCreateInfo pci{}; pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = queueFamily_;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device_, &pci, nullptr, &cmdPool_);
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device_, &ai, &cmd_);

    VkSemaphoreCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device_, &sci, nullptr, &imageAvailable_);
    vkCreateSemaphore(device_, &sci, nullptr, &renderFinished_);
    VkFenceCreateInfo fci{}; fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device_, &fci, nullptr, &fence_);

    if (!createPipeline()) { UI_LOG_ERROR("pipeline creation failed"); return false; }

    VkDescriptorPoolSize ps{}; ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 256;
    VkDescriptorPoolCreateInfo dpci{}; dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 256; dpci.poolSizeCount = 1; dpci.pPoolSizes = &ps;
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (vkCreateDescriptorPool(device_, &dpci, nullptr, &descPool_) != VK_SUCCESS)
        return false;

    const uint8_t white[4] = {255, 255, 255, 255};
    whiteTexId_ = createTextureInternal(1, 1, white);

    ready_ = true;
    UI_LOG_INFO("Vulkan device ready (%dx%d)", w, h);
    return true;
}

bool VKDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size; bi.usage = usage; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &buf) != VK_SUCCESS) return false;
    VkMemoryRequirements req; vkGetBufferMemoryRequirements(device_, buf, &req);
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(physical_, req.memoryTypeBits, props);
    if (ai.memoryTypeIndex == UINT32_MAX) return false;
    if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(device_, buf, mem, 0);
    return true;
}

VkCommandBuffer VKDevice::beginOneShot() {
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(device_, &ai, &cb);
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

void VKDevice::endOneShot(VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue_);
    vkFreeCommandBuffers(device_, cmdPool_, 1, &cb);
}

void VKDevice::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                     VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = oldLayout; b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        b.srcAccessMask = 0; b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else {
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &b);
}

bool VKDevice::createPipeline() {
    auto loadShader = [&](const uint32_t* code, size_t bytes) {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = bytes; ci.pCode = code;
        VkShaderModule m;
        vkCreateShaderModule(device_, &ci, nullptr, &m);
        return m;
    };
    VkShaderModule vs = loadShader(kUI_Vert_SPV, sizeof(kUI_Vert_SPV));
    VkShaderModule fs = loadShader(kUI_Frag_SPV, sizeof(kUI_Frag_SPV));

    VkSamplerCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.anisotropyEnable = VK_FALSE; sci.maxAnisotropy = 1.0f;
    sci.maxLod = 1.0f;
    vkCreateSampler(device_, &sci, nullptr, &sampler_);

    VkDescriptorSetLayoutBinding b{};
    b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1; b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dli{};
    dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dli.bindingCount = 1; dli.pBindings = &b;
    vkCreateDescriptorSetLayout(device_, &dli, nullptr, &descLayout_);

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0; pc.size = 56;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1; pli.pSetLayouts = &descLayout_;
    pli.pushConstantRangeCount = 1; pli.pPushConstantRanges = &pc;
    vkCreatePipelineLayout(device_, &pli, nullptr, &pipelineLayout_);

    VkVertexInputBindingDescription vb{};
    vb.binding = 0; vb.stride = sizeof(Vertex);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[3] = {};
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32_SFLOAT; attrs[1].offset = 8;
    attrs[2].location = 2; attrs[2].binding = 0; attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[2].offset = 16;

    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount = 1; vis.pVertexBindingDescriptions = &vb;
    vis.vertexAttributeDescriptionCount = 3; vis.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ias{};
    ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ias.primitiveRestartEnable = VK_FALSE;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vs; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyns{};
    dyns.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyns.dynamicStateCount = 2; dyns.pDynamicStates = dyn;

    VkPipelineViewportStateCreateInfo vps{};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1; vps.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable = VK_FALSE; rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = msaaSamples_;
    ms.minSampleShading = 1.0f;

    VkPipelineDepthStencilStateCreateInfo dss{};
    dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dss.depthTestEnable = VK_FALSE; dss.depthWriteEnable = VK_FALSE;
    dss.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState cb{};
    cb.blendEnable = VK_TRUE;
    cb.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.logicOpEnable = VK_FALSE;
    cbs.attachmentCount = 1; cbs.pAttachments = &cb;

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount = 2; gpi.pStages = stages;
    gpi.pVertexInputState = &vis;
    gpi.pInputAssemblyState = &ias;
    gpi.pViewportState = &vps;
    gpi.pRasterizationState = &rs;
    gpi.pMultisampleState = &ms;
    gpi.pDepthStencilState = &dss;
    gpi.pColorBlendState = &cbs;
    gpi.pDynamicState = &dyns;
    gpi.layout = pipelineLayout_;
    gpi.renderPass = renderPass_;
    gpi.subpass = 0;

    bool ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gpi, nullptr, &pipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, vs, nullptr);
    vkDestroyShaderModule(device_, fs, nullptr);
    if (!ok) {
        UI_LOG_ERROR("vkCreateGraphicsPipelines failed");
        return false;
    }
    return true;
}

uint32_t VKDevice::createTexture(int w, int h, const uint8_t* rgba) {
    if (rec_) {
        // 录制模式：只分配逻辑 id + 记命令（像素拷入命令载荷），GPU
        // 上传与对象创建由渲染线程 replay 执行——计算线程零 VK 调用。
        // texCmdsRec_ 跨帧累积（两帧之间的图集烘焙同样覆盖），
        // present 时整批移交帧包，帧外命令不丢失
        FramePacket::VKTexCmd tc;
        tc.destroy = false;
        tc.id = nextTexId_++;   // 逻辑 id：录制模式下仅计算线程写 nextTexId_
        tc.w = w; tc.h = h;
        tc.rgba.assign(rgba, rgba + (size_t)w * h * 4);
        texCmdsRec_.push_back(std::move(tc));
        return tc.id;
    }
    return createTextureInternal(w, h, rgba);
}

uint32_t VKDevice::createTextureInternal(int w, int h, const uint8_t* rgba,
                                         uint32_t forceId) {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = { (uint32_t)w, (uint32_t)h, 1 };
    ii.mipLevels = 1; ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &ii, nullptr, &image) != VK_SUCCESS) return 0;

    VkMemoryRequirements req; vkGetImageMemoryRequirements(device_, image, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(physical_, req.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) return 0;
    vkBindImageMemory(device_, image, mem, 0);

    VkDeviceSize size = (VkDeviceSize)w * h * 4;
    VkBuffer sbuf = VK_NULL_HANDLE; VkDeviceMemory smem = VK_NULL_HANDLE;
    if (!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      sbuf, smem)) return 0;
    void* p = nullptr;
    vkMapMemory(device_, smem, 0, size, 0, &p);
    std::memcpy(p, rgba, (size_t)size);
    vkUnmapMemory(device_, smem);

    VkCommandBuffer cb = beginOneShot();
    transitionImageLayout(cb, image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
    vkCmdCopyBufferToImage(cb, sbuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transitionImageLayout(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    endOneShot(cb);

    vkDestroyBuffer(device_, sbuf, nullptr);
    vkFreeMemory(device_, smem, nullptr);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &vi, nullptr, &view) != VK_SUCCESS) return 0;

    VkDescriptorSet dset = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = descPool_; dai.descriptorSetCount = 1;
    dai.pSetLayouts = &descLayout_;
    if (vkAllocateDescriptorSets(device_, &dai, &dset) != VK_SUCCESS) return 0;
    VkDescriptorImageInfo dimg{};
    dimg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dimg.imageView = view; dimg.sampler = sampler_;
    VkWriteDescriptorSet wd{};
    wd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wd.dstSet = dset; wd.dstBinding = 0; wd.descriptorCount = 1;
    wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wd.pImageInfo = &dimg;
    vkUpdateDescriptorSets(device_, 1, &wd, 0, nullptr);

    // id 分配：回放路径用包内 id（录制侧已分配，保持两线程 id 空间
    // 一致）；直通路径自增
    uint32_t id = forceId ? forceId : nextTexId_++;
    Texture t;
    t.image = image; t.mem = mem; t.view = view; t.dset = dset;
    textures_[id] = t;
    return id;
}

void VKDevice::destroyTexture(uint32_t id) {
    if (rec_) {
        // 录制模式：销毁命令随帧包走（present 移交），回放时摘除映射
        // 挂 retired_ 延迟释放——与直通 destroyTexture 同一安全策略
        FramePacket::VKTexCmd tc;
        tc.destroy = true;
        tc.id = id;
        texCmdsRec_.push_back(std::move(tc));
        return;
    }
    auto it = textures_.find(id);
    if (it == textures_.end()) return;
    // 延迟释放：drawTriangles 是延迟批量（present 才消费 cmds_），
    // 帧内 rebuild atlas 会先销毁旧纹理再建新纹理，立即释放会使
    // cmds_ 中已记录的 dset 悬垂 → vkCmdBindDescriptorSets 驱动崩溃。
    // 因此帧内只摘除映射，资源挂入 retired_，present 提交后统一释放。
    retired_.push_back(it->second);
    textures_.erase(it);
}

void VKDevice::freeTexture(const Texture& t) {
    if (t.dset) vkFreeDescriptorSets(device_, descPool_, 1, &t.dset);
    if (t.view) vkDestroyImageView(device_, t.view, nullptr);
    if (t.image) vkDestroyImage(device_, t.image, nullptr);
    if (t.mem) vkFreeMemory(device_, t.mem, nullptr);
}

void VKDevice::flushRetired() {
    if (retired_.empty()) return;
    vkDeviceWaitIdle(device_);
    for (auto& t : retired_) freeTexture(t);
    retired_.clear();
}

void VKDevice::setSDFParams(int mode, float x, float y, float w, float h,
                            float rTL, float rTR, float rBR, float rBL, float thickness) {
    sdfMode_ = mode;
    sdfX_ = x; sdfY_ = y; sdfW_ = w; sdfH_ = h;
    sdfRadiusTL_ = rTL; sdfRadiusTR_ = rTR; sdfRadiusBR_ = rBR; sdfRadiusBL_ = rBL;
    sdfThickness_ = thickness;
}

void VKDevice::setAAEnabled(bool on) { aaOn_ = on ? 1.0f : 0.0f; }

// 渲染目标逻辑尺寸（纯数据写入：跨线程安全，离屏重建延迟到 presentImpl）
void VKDevice::setTargetSize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == tw_ && h == th_)) return;
    tw_ = w; th_ = h;
    targetDirty_.store(true, std::memory_order_release);   // 渲染线程消费
}

void VKDevice::setScissorRect(int x, int y, int w, int h) {
    // 批量延迟：仅记录当前状态，present 录制时按 Cmd 快照生效
    scCur_ = {x, y, w, h};
    scOn_ = true;
}

void VKDevice::resetScissorRect() {
    scOn_ = false;
}

void VKDevice::pushScissorRect(int x, int y, int w, int h) {
    // 嵌套裁剪求交（与 GLDevice 单源语义一致）：见 GLDevice::pushScissorRect 注释
    if (!scStack_.empty()) {
        const int px = scStack_.back().x, py = scStack_.back().y;
        const int pw = scStack_.back().w, ph = scStack_.back().h;
        int x1 = x > px ? x : px;
        int y1 = y > py ? y : py;
        int x2 = (x + w) < (px + pw) ? (x + w) : (px + pw);
        int y2 = (y + h) < (py + ph) ? (y + h) : (py + ph);
        if (x2 < x1) x2 = x1;
        if (y2 < y1) y2 = y1;
        x = x1; y = y1; w = x2 - x1; h = y2 - y1;
    }
    scStack_.push_back({x, y, w, h});
    setScissorRect(x, y, w, h);
}

void VKDevice::popScissorRect() {
    if (!scStack_.empty()) scStack_.pop_back();
    if (scStack_.empty()) {
        resetScissorRect();
    } else {
        const ScRect& t = scStack_.back();
        setScissorRect(t.x, t.y, t.w, t.h);
    }
}

void VKDevice::drawTriangles(const Vertex* verts, int vcount,
                             const uint32_t* indices, int icount,
                             uint32_t textureId, bool textured) {
    if (!ready_ || vcount <= 0 || icount <= 0) return;
    // 目标选择：录制模式写帧包（渲染线程回放），直通写成员（present 消费）。
    // 两路径命令生成逻辑完全一致（含合批判断），仅数据落点不同 → 逐位一致。
    // 录制模式下本函数零 VK 调用、不读 textures_ 映射（渲染线程所有）
    std::vector<Vertex>&              dv = rec_ ? rec_->verts   : verts_;
    std::vector<uint32_t>&            di = rec_ ? rec_->indices : indices_;
    std::vector<FramePacket::VKCmd>&  dc = rec_ ? rec_->vkCmds  : cmds_;

    FramePacket::VKCmd c;
    c.indexOffset = (uint32_t)di.size();
    c.indexCount = (uint32_t)icount;
    c.scissorOn = scOn_ ? 1 : 0;
    // 裁剪快照（离屏 1:1：逻辑坐标原值；VK 原点左上无需翻转）
    c.scX = scCur_.x; c.scY = scCur_.y;
    c.scW = scCur_.w; c.scH = scCur_.h;

    // 逻辑纹理 id：0 = 白纹理（textured=false 或 id 无效回退，回退判定
    // 挪到呈现时 presentImpl 按 textures_ 查找——录制侧不碰映射）
    c.texId = (textured && textureId) ? textureId : 0;
    c.sdfMode = sdfMode_;
    // SDF 快照原值（离屏 1:1 光栅化，present 时直接 push 常量）
    c.sdfX = sdfX_; c.sdfY = sdfY_;
    c.sdfW = sdfW_; c.sdfH = sdfH_;
    c.sdfRadiusTL = sdfRadiusTL_; c.sdfRadiusTR = sdfRadiusTR_;
    c.sdfRadiusBR = sdfRadiusBR_; c.sdfRadiusBL = sdfRadiusBL_;
    c.sdfThickness = sdfThickness_;
    c.aaOn = aaOn_;

    uint32_t base = (uint32_t)dv.size();
    dv.reserve(dv.size() + vcount);
    for (int i = 0; i < vcount; ++i) dv.push_back(verts[i]);   // 离屏 1:1 零缩放
    di.reserve(di.size() + icount);
    for (int i = 0; i < icount; ++i) di.push_back(base + indices[i]);

    // 【性能优化·帧内合批】非 SDF 绘制且尾 Cmd 同纹理/同裁剪/非 SDF →
    // 并入尾 Cmd（indexCount 累加即可：尾 Cmd 索引段必然连续延伸到
    // 索引池尾部——本函数只追加、Cmd 按序生成，中无空洞）。
    // 与 GL 侧合批同策略：文字（同图集）与纯色（同白纹理）各成一批，
    // SDF 因 per-draw push 常量不可合并。合批比较逻辑纹理 id（非 dset）
    // ——录制/直通两路径统一按 texId 判重，呈现时才解析 dset，
    // 保证回放与直通产生完全相同的提交序列。
    if (c.sdfMode == 0 && !dc.empty()) {
        FramePacket::VKCmd& b = dc.back();
        if (b.sdfMode == 0 && b.texId == c.texId &&
            b.scissorOn == c.scissorOn &&
            (!c.scissorOn || (b.scX == c.scX && b.scY == c.scY &&
                              b.scW == c.scW && b.scH == c.scH))) {
            b.indexCount += c.indexCount;
            return;
        }
    }
    dc.push_back(c);
}

void VKDevice::ensureBuffers(VkDeviceSize vbytes, VkDeviceSize ibytes) {
    if (vbytes > vboCapacity_) {
        if (vbo_) { vkDestroyBuffer(device_, vbo_, nullptr); vkFreeMemory(device_, vboMem_, nullptr); }
        VkDeviceSize cap = 65536;
        while (cap < vbytes) cap <<= 1;
        createBuffer(cap, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vbo_, vboMem_);
        vkMapMemory(device_, vboMem_, 0, cap, 0, &vboMapped_);
        vboCapacity_ = cap;
    }
    if (ibytes > iboCapacity_) {
        if (ibo_) { vkDestroyBuffer(device_, ibo_, nullptr); vkFreeMemory(device_, iboMem_, nullptr); }
        VkDeviceSize cap = 65536;
        while (cap < ibytes) cap <<= 1;
        createBuffer(cap, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     ibo_, iboMem_);
        vkMapMemory(device_, iboMem_, 0, cap, 0, &iboMapped_);
        iboCapacity_ = cap;
    }
}

bool VKDevice::recreate() {
    VkSurfaceCapabilitiesKHR cap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &cap);
    extent_ = cap.currentExtent;
    if (extent_.width == 0xFFFFFFFF) { extent_.width = 1280; extent_.height = 720; }

    uint32_t fc = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &fc, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fc);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &fc, fmts.data());
    format_ = fmts[0].format;

    VkPhysicalDeviceProperties pdp;
    vkGetPhysicalDeviceProperties(physical_, &pdp);
    msaaSamples_ = VK_SAMPLE_COUNT_4_BIT;
    if (!(pdp.limits.framebufferColorSampleCounts & msaaSamples_))
        msaaSamples_ = VK_SAMPLE_COUNT_2_BIT;
    if (!(pdp.limits.framebufferColorSampleCounts & msaaSamples_))
        msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;

    VkSwapchainCreateInfoKHR sci{}; sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface_;
    // ---- Image 数量（不锁帧关键之一） ----
    // MAILBOX 语义需要 ≥3：2 个 image 时合成器持有 1 个、渲染只剩 1 个，
    // present 后再 acquire 必须等合成器释放（= 锁刷新率；实测前台 60fps/
    // 最小化 3000fps 的根因）。取 max(3, surface 最小值)，受 maxImageCount
    // 上限约束（0 = 无上限）；FIFO 模式多余 image 只增加延迟不锁帧。
    {
        uint32_t want = std::max(3u, cap.minImageCount);
        if (cap.maxImageCount > 0 && want > cap.maxImageCount) want = cap.maxImageCount;
        sci.minImageCount = want;
    }
    sci.imageFormat = format_; sci.imageColorSpace = fmts[0].colorSpace;
    sci.imageExtent = extent_; sci.imageArrayLayers = 1;
    // TRANSFER_SRC：引擎内截图（requestScreenshot）blit 源需求；
    // TRANSFER_DST：离屏渲染结果放大 blit 的目标需求（真·分辨率降档）
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = cap.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // ---- Present mode 选择（不锁帧优先，UI_VSYNC=1 恢复垂直同步）----
    // FIFO = 队列等待刷新率（锁 60/120fps 的元凶）。默认走不锁帧链：
    // MAILBOX（三重缓冲，无撕裂不锁上限）→ IMMEDIATE（立即呈现，可能
    // 撕裂）→ FIFO（规范保证可用的最终回落）。选择基于设备实际支持
    // 列表查询，非硬编码假设——扩展性：改优先级只动此数组。
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        const char* vs = std::getenv("UI_VSYNC");
        const bool wantVsync = vs && vs[0] == '1';
        if (!wantVsync) {
            uint32_t pmc = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &pmc, nullptr);
            std::vector<VkPresentModeKHR> modes(pmc);
            if (pmc > 0 &&
                vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &pmc, modes.data()) == VK_SUCCESS) {
                const VkPresentModeKHR pref[] = { VK_PRESENT_MODE_MAILBOX_KHR,
                                                 VK_PRESENT_MODE_IMMEDIATE_KHR };
                for (VkPresentModeKHR want : pref) {
                    bool found = false;
                    for (VkPresentModeKHR m : modes)
                        if (m == want) { sci.presentMode = m; found = true; break; }
                    if (found) break;
                }
            }
        }
    }
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) return false;

    uint32_t ic = 0; vkGetSwapchainImagesKHR(device_, swapchain_, &ic, nullptr);
    images_.resize(ic); vkGetSwapchainImagesKHR(device_, swapchain_, &ic, images_.data());
    // 交换链 image 仅作 blit 目标 + present：不再需要 per-image view/fb
    //（离屏渲染唯一 fb 由 renderPass 就绪后 ensureOffscreen 创建）

    // ---- renderPass：att0=MSAA 渲染、att1=resolve 到离屏 offscreen ----
    // offscreen 的 finalLayout=TRANSFER_SRC（presentImpl 直接 blit 到交换链）
    VkAttachmentDescription atts[2] = {};
    atts[0].format = format_;
    atts[0].samples = msaaSamples_;
    atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    atts[1].format = format_;
    atts[1].samples = VK_SAMPLE_COUNT_1_BIT;
    atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[1].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference colorRef{}; colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference resolveRef{}; resolveRef.attachment = 1;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription sp{}; sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1; sp.pColorAttachments = &colorRef;
    sp.pResolveAttachments = &resolveRef;
    VkRenderPassCreateInfo rpi{}; rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpi.attachmentCount = 2; rpi.pAttachments = atts;
    rpi.subpassCount = 1; rpi.pSubpasses = &sp;
    vkCreateRenderPass(device_, &rpi, nullptr, &renderPass_);

    // 离屏目标（image 幂等：尺寸未变跳过重建）+ 唯一 fb（renderPass 就绪）
    ensureOffscreen();
    return true;
}

// ============================================================
// 离屏渲染目标（真·分辨率降档核心）
//
// 资源：msaaImage_（MSAA 渲染附件）+ offscreenImage_（resolve 目标，
// 亦即 presentImpl blit 到交换链的源）+ fb_（唯一离屏 framebuffer）。
// 尺寸 = tw_ x th_（逻辑尺寸：renderRes 强制档=查表值；跟随窗口=物理），
// 与交换链尺寸解耦——交换链重建/窗口 resize 不触发本函数重建（幂等），
// setTargetSize 尺寸变化才重建（presentImpl 渲染线程 GPU 空闲点消费）。
// fb_ 依赖 renderPass_：recreate() 重建 rp 后必调本函数重建 fb。
// ============================================================
void VKDevice::ensureOffscreen() {
    if (!device_) return;
    if (tw_ <= 0 || th_ <= 0) { tw_ = (int)extent_.width; th_ = (int)extent_.height; }
    const uint32_t w = (uint32_t)tw_, h = (uint32_t)th_;

    // ---- image/view：尺寸未变且句柄有效 → 跳过（recreate 重入幂等）----
    if (msaaImage_ == VK_NULL_HANDLE || offscreenImage_ == VK_NULL_HANDLE ||
        offscreenW_ != w || offscreenH_ != h) {
        // GPU 空闲点重建（presentImpl fence 已等 / recreate 单线程独占）
        vkDeviceWaitIdle(device_);
        if (msaaView_) vkDestroyImageView(device_, msaaView_, nullptr);
        if (msaaImage_) vkDestroyImage(device_, msaaImage_, nullptr);
        if (msaaMem_) vkFreeMemory(device_, msaaMem_, nullptr);
        if (offscreenView_) vkDestroyImageView(device_, offscreenView_, nullptr);
        if (offscreenImage_) vkDestroyImage(device_, offscreenImage_, nullptr);
        if (offscreenMem_) vkFreeMemory(device_, offscreenMem_, nullptr);
        msaaView_ = VK_NULL_HANDLE; msaaImage_ = VK_NULL_HANDLE; msaaMem_ = VK_NULL_HANDLE;
        offscreenView_ = VK_NULL_HANDLE; offscreenImage_ = VK_NULL_HANDLE;
        offscreenMem_ = VK_NULL_HANDLE;

        // MSAA 渲染附件（样本数与旧交换链直通路径一致，视觉不变）
        if (msaaSamples_ != VK_SAMPLE_COUNT_1_BIT) {
            VkImageCreateInfo mii{}; mii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            mii.imageType = VK_IMAGE_TYPE_2D;
            mii.format = format_;
            mii.extent = { w, h, 1 };
            mii.mipLevels = 1; mii.arrayLayers = 1;
            mii.samples = msaaSamples_;
            mii.tiling = VK_IMAGE_TILING_OPTIMAL;
            mii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            mii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            mii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkCreateImage(device_, &mii, nullptr, &msaaImage_);
            VkMemoryRequirements req; vkGetImageMemoryRequirements(device_, msaaImage_, &req);
            VkMemoryAllocateInfo mai{}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            mai.allocationSize = req.size;
            mai.memoryTypeIndex = findMemoryType(physical_, req.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(device_, &mai, nullptr, &msaaMem_);
            vkBindImageMemory(device_, msaaImage_, msaaMem_, 0);
            VkImageViewCreateInfo mvi{}; mvi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            mvi.image = msaaImage_; mvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            mvi.format = format_; mvi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mvi.subresourceRange.levelCount = 1; mvi.subresourceRange.layerCount = 1;
            vkCreateImageView(device_, &mvi, nullptr, &msaaView_);
        }

        // 离屏 resolve 目标（blit 源需要 TRANSFER_SRC usage）
        VkImageCreateInfo oii{}; oii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        oii.imageType = VK_IMAGE_TYPE_2D;
        oii.format = format_;
        oii.extent = { w, h, 1 };
        oii.mipLevels = 1; oii.arrayLayers = 1;
        oii.samples = VK_SAMPLE_COUNT_1_BIT;
        oii.tiling = VK_IMAGE_TILING_OPTIMAL;
        oii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        oii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        oii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCreateImage(device_, &oii, nullptr, &offscreenImage_);
        VkMemoryRequirements oreq; vkGetImageMemoryRequirements(device_, offscreenImage_, &oreq);
        VkMemoryAllocateInfo omai{}; omai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        omai.allocationSize = oreq.size;
        omai.memoryTypeIndex = findMemoryType(physical_, oreq.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device_, &omai, nullptr, &offscreenMem_);
        vkBindImageMemory(device_, offscreenImage_, offscreenMem_, 0);
        VkImageViewCreateInfo ovi{}; ovi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ovi.image = offscreenImage_; ovi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ovi.format = format_; ovi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ovi.subresourceRange.levelCount = 1; ovi.subresourceRange.layerCount = 1;
        vkCreateImageView(device_, &ovi, nullptr, &offscreenView_);
        offscreenW_ = w; offscreenH_ = h;
        UI_LOG_INFO("vk offscreen target %ux%u (msaa=%d)", w, h, (int)msaaSamples_);
    }

    // ---- fb：renderPass 就绪即重建（rp 重建后必须跟着重建）----
    if (renderPass_ != VK_NULL_HANDLE) {
        if (fb_) vkDestroyFramebuffer(device_, fb_, nullptr);
        VkImageView fbAtts[2] = { msaaView_, offscreenView_ };
        VkFramebufferCreateInfo fbi{}; fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = renderPass_; fbi.attachmentCount = 2; fbi.pAttachments = fbAtts;
        fbi.width = w; fbi.height = h; fbi.layers = 1;
        vkCreateFramebuffer(device_, &fbi, nullptr, &fb_);
    }
}

void VKDevice::destroySwapchainDeps() {
    // fb 依赖 renderPass：先 fb 后 rp（离屏 fb 不绑交换链 image，但随
    // renderPass 生命周期重建；msaa/offscreen image 归 ensureOffscreen
    // 管理，不随交换链销毁——尺寸与交换链解耦）
    if (fb_) vkDestroyFramebuffer(device_, fb_, nullptr);
    fb_ = VK_NULL_HANDLE;

    images_.clear();
    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

void VKDevice::shutdown() {
    if (!instance_) return;
    if (device_) vkDeviceWaitIdle(device_);

    for (auto& kv : textures_) freeTexture(kv.second);
    textures_.clear();
    for (auto& t : retired_) freeTexture(t);
    retired_.clear();

    if (vbo_) vkDestroyBuffer(device_, vbo_, nullptr);
    if (vboMem_) vkFreeMemory(device_, vboMem_, nullptr);
    if (ibo_) vkDestroyBuffer(device_, ibo_, nullptr);
    if (iboMem_) vkFreeMemory(device_, iboMem_, nullptr);
    vbo_ = ibo_ = VK_NULL_HANDLE; vboMapped_ = iboMapped_ = nullptr;

    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (descLayout_) vkDestroyDescriptorSetLayout(device_, descLayout_, nullptr);
    if (descPool_) vkDestroyDescriptorPool(device_, descPool_, nullptr);
    if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    descLayout_ = VK_NULL_HANDLE;
    descPool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;

    if (fence_) vkDestroyFence(device_, fence_, nullptr);
    if (renderFinished_) vkDestroySemaphore(device_, renderFinished_, nullptr);
    if (imageAvailable_) vkDestroySemaphore(device_, imageAvailable_, nullptr);
    if (cmdPool_) vkDestroyCommandPool(device_, cmdPool_, nullptr);
    // 离屏渲染目标（独立于交换链生命周期，最终统一回收）
    if (fb_) vkDestroyFramebuffer(device_, fb_, nullptr);
    fb_ = VK_NULL_HANDLE;
    if (msaaView_) vkDestroyImageView(device_, msaaView_, nullptr);
    if (msaaImage_) vkDestroyImage(device_, msaaImage_, nullptr);
    if (msaaMem_) vkFreeMemory(device_, msaaMem_, nullptr);
    msaaView_ = VK_NULL_HANDLE; msaaImage_ = VK_NULL_HANDLE; msaaMem_ = VK_NULL_HANDLE;
    if (offscreenView_) vkDestroyImageView(device_, offscreenView_, nullptr);
    if (offscreenImage_) vkDestroyImage(device_, offscreenImage_, nullptr);
    if (offscreenMem_) vkFreeMemory(device_, offscreenMem_, nullptr);
    offscreenView_ = VK_NULL_HANDLE; offscreenImage_ = VK_NULL_HANDLE;
    offscreenMem_ = VK_NULL_HANDLE;
    // 截图 staging（懒创建资源，随设备销毁）
    if (shotImg_ != VK_NULL_HANDLE) vkDestroyImage(device_, shotImg_, nullptr);
    if (shotMem_ != VK_NULL_HANDLE) vkFreeMemory(device_, shotMem_, nullptr);
    shotImg_ = VK_NULL_HANDLE; shotMem_ = VK_NULL_HANDLE;
    destroySwapchainDeps();
    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE; ready_ = false;
}

void VKDevice::resize(int w, int h) {
    if (!ready_) return;
    if (ThreadScheduler::get().multithread()) {
        // 多线程：渲染线程可能正在回放本设备（presentImpl 提交中），
        // 立即 vkDeviceWaitIdle + 销毁交换链会与回放竞争。只挂请求，
        // presentImpl 开头由渲染线程串行消费（回放本身就在渲染线程，
        // 与提交天然互斥；recreate 内部从 surface capabilities 取新尺寸）
        (void)w; (void)h;
        resizePending_ = true;
        return;
    }
    vkDeviceWaitIdle(device_);
    destroySwapchainDeps();
    recreate();
}

void VKDevice::beginFrame() {
    if (rec_) {
        // 录制模式：清帧包 VK 载荷（RenderThread acquire 已整包 reset，
        // 此处与直通路径对称清几何）。texCmdsRec_ 不清——帧外（两帧
        // 之间）烘焙产生的纹理命令必须随下一包移交，否则纹理丢失
        rec_->vkCmds.clear();
        rec_->verts.clear();
        rec_->indices.clear();
    } else {
        verts_.clear(); indices_.clear(); cmds_.clear();
    }
    scStack_.clear(); scOn_ = false;   // 帧级防御：裁剪栈失衡不跨帧累积
}
void VKDevice::endFrame()   {}

void VKDevice::setRecording(FramePacket* pkt) {
    // 切换录制目标。texCmdsRec_ 刻意不清空：模式切换（pause 同步点）
    // 时若有未移交的纹理命令，保留到下一次录制随包移交——这些命令
    // 对应的逻辑 id 已返回给调用方（字体图集等），丢弃创建命令会让
    // 后续绘制永久回退白纹理（文字消失）
    rec_ = pkt;
}

void VKDevice::present() {
    if (!ready_) return;
    if (rec_) {
        // 录制模式：VK 载荷打包移交（清屏色快照 + 帧内外累积的纹理
        // 命令），GPU 提交由渲染线程 replay 执行——本函数零 VK 调用。
        // move 后显式 clear 保证源容器回到确定的空状态
        rec_->vkCr = clearColor_.r; rec_->vkCg = clearColor_.g;
        rec_->vkCb = clearColor_.b; rec_->vkCa = clearColor_.a;
        rec_->vkTexCmds = std::move(texCmdsRec_);
        texCmdsRec_.clear();
        return;
    }
    presentImpl(verts_, indices_, cmds_, clearColor_);
}

// ============================================================
// 帧提交实现（渲染线程执行；数据源参数化）
//
// 直通路径：数据来自成员（verts_/indices_/cmds_，主线程 present 调用）；
// 回放路径：数据来自帧包（replay 转发，渲染线程调用）。
// 两路径执行完全相同的 VK 调用序列 → 渲染效果逐位一致。
// dset 解析统一在此处按逻辑 texId 查 textures_（0=白纹理；查不到
// 回退白纹理——与旧直通路径的回退语义一致）。
// ============================================================
void VKDevice::presentImpl(const std::vector<Vertex>& verts,
                           const std::vector<uint32_t>& indices,
                           const std::vector<FramePacket::VKCmd>& cmds,
                           const Color& clearColor) {
    // 多线程 resize 请求消费（渲染线程串行执行——与回放/提交天然互斥；
    // 单线程路径 resize() 已立即重建，此处恒 false 零开销）
    if (resizePending_.exchange(false)) {
        vkDeviceWaitIdle(device_);
        destroySwapchainDeps();
        recreate();
    }
    uint32_t idx = 0;
    VkResult r = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        imageAvailable_, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        resize(extent_.width, extent_.height); return;
    }
    if (r != VK_SUCCESS) return;

    vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &fence_);

    // 渲染目标尺寸变更消费（setTargetSize 跨线程置位；此处 GPU 空闲——
    // fence 已等，ensureOffscreen 内部重建 msaa/offscreen + fb）
    if (targetDirty_.exchange(false, std::memory_order_acquire))
        ensureOffscreen();
    if (fb_ == VK_NULL_HANDLE) return;   // 离屏目标缺失：跳过本帧（异常防御）

    if (!verts.empty() && !indices.empty()) {
        VkDeviceSize vbytes = verts.size() * sizeof(Vertex);
        VkDeviceSize ibytes = indices.size() * sizeof(uint32_t);
        ensureBuffers(vbytes, ibytes);
        if (!vboMapped_ || !iboMapped_) return;   // 缓冲分配失败：跳过本帧
        std::memcpy(vboMapped_, verts.data(), (size_t)vbytes);
        std::memcpy(iboMapped_, indices.data(), (size_t)ibytes);
    }

    vkResetCommandBuffer(cmd_, 0);
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd_, &bi);
    VkClearValue cv{}; cv.color.float32[0] = clearColor.r;
    cv.color.float32[1] = clearColor.g; cv.color.float32[2] = clearColor.b;
    cv.color.float32[3] = clearColor.a;
    VkRenderPassBeginInfo rpbi{}; rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = renderPass_; rpbi.framebuffer = fb_;   // 离屏 fb（逻辑尺寸）
    rpbi.renderArea.extent = { (uint32_t)tw_, (uint32_t)th_ };
    rpbi.clearValueCount = 1; rpbi.pClearValues = &cv;
    vkCmdBeginRenderPass(cmd_, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport vp{};
    vp.width = (float)tw_; vp.height = (float)th_;           // 渲染目标=离屏逻辑尺寸
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_, 0, 1, &vp);
    VkRect2D sc{}; sc.extent = { (uint32_t)tw_, (uint32_t)th_ };
    vkCmdSetScissor(cmd_, 0, 1, &sc);

    if (!verts.empty() && !indices.empty()) {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmd_, 0, 1, &vbo_, offsets);
        vkCmdBindIndexBuffer(cmd_, ibo_, 0, VK_INDEX_TYPE_UINT32);

        struct PushData {
            float resX, resY;
            int   sdfMode;
            int   _pad;
            float rectX, rectY, rectW, rectH;
            float radiusTL, radiusTR, radiusBR, radiusBL;
            float borderThickness;
            float aaOn;
        } pd;
        pd.resX = (float)tw_;               // NDC 基准=离屏逻辑尺寸
        pd.resY = (float)th_;
        pd._pad = 0;
        for (auto& c : cmds) {
            // 逻辑纹理 id → dset（渲染线程侧映射；0=白纹理；
            // 查不到回退白纹理，连白纹理都没有则丢弃该批——异常态防御）
            uint32_t tid = c.texId ? c.texId : whiteTexId_;
            auto it = textures_.find(tid);
            if (it == textures_.end()) {
                it = textures_.find(whiteTexId_);
                if (it == textures_.end()) continue;
            }
            pd.sdfMode = c.sdfMode;
            pd.rectX = c.sdfX; pd.rectY = c.sdfY;
            pd.rectW = c.sdfW; pd.rectH = c.sdfH;
            pd.radiusTL = c.sdfRadiusTL; pd.radiusTR = c.sdfRadiusTR;
            pd.radiusBR = c.sdfRadiusBR; pd.radiusBL = c.sdfRadiusBL;
            pd.borderThickness = c.sdfThickness;
            pd.aaOn = c.aaOn;
            vkCmdPushConstants(cmd_, pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, 56, &pd);
            // 裁剪：按 Cmd 快照设置（VK 帧缓冲原点左上，无需 Y 翻转；
            // 手动 clamp 到离屏目标范围，完全在外 → 0 尺寸丢弃）
            VkRect2D rs{};
            if (c.scissorOn) {
                int sx = c.scX < 0 ? 0 : c.scX;
                int sy = c.scY < 0 ? 0 : c.scY;
                int ex = c.scX + c.scW; if (ex > tw_) ex = tw_;
                int ey = c.scY + c.scH; if (ey > th_) ey = th_;
                if (ex > sx && ey > sy) {
                    rs.offset = {sx, sy};
                    rs.extent = {(uint32_t)(ex - sx), (uint32_t)(ey - sy)};
                }
            } else {
                rs.extent = { (uint32_t)tw_, (uint32_t)th_ };
            }
            vkCmdSetScissor(cmd_, 0, 1, &rs);
            vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                    0, 1, &it->second.dset, 0, nullptr);
            vkCmdDrawIndexed(cmd_, c.indexCount, 1, c.indexOffset, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd_);

    // ---- 离屏画面 → 交换链 image（真·分辨率降档的呈现通路）----
    // offscreen 已由 renderPass finalLayout 转 TRANSFER_SRC（零屏障）；
    // 交换链 image acquire 后布局未定（UNDEFINED/PRESENT_SRC 均可能），
    // 用 UNDEFINED→TRANSFER_DST 转换（内容不保——本帧整体覆盖 blit）。
    // 1:1（跟随窗口）= NEAREST 逐位一致；非 1:1（强制档）= LINEAR 缩放。
    {
        const bool same = ((uint32_t)tw_ == extent_.width && (uint32_t)th_ == extent_.height);
        VkImageMemoryBarrier toDst{};
        toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toDst.srcAccessMask = 0;
        toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.image = images_[idx];
        toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toDst);
        VkImageBlit blit{};
        blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { tw_, th_, 1 };
        blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { (int32_t)extent_.width, (int32_t)extent_.height, 1 };
        vkCmdBlitImage(cmd_, offscreenImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       images_[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, same ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = images_[idx];
        toPresent.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toPresent);
    }

    // ---- 截图消费（请求标记原子取走）：renderPass 完成后 blit 交换链
    //      image → 线性 staging；present 提交后等 fence 读回落盘。
    //      放 renderPass 后保证读的是本帧完成内容；blit 前后布局屏障
    //      转换（PRESENT_SRC ↔ TRANSFER_SRC）。----
    const bool wantShot = shotReq_.exchange(false, std::memory_order_acquire);
    if (wantShot) {
        ensureShotStaging();
        if (shotImg_ != VK_NULL_HANDLE) {
            VkImageMemoryBarrier b[2]{};
            b[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            b[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            b[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            b[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b[0].image = images_[idx];
            b[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            b[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b[1].srcAccessMask = 0;
            b[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b[1].image = shotImg_;
            b[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                 0, nullptr, 0, nullptr, 2, b);
            // blit 正序（VK image 坐标原点在左上——线性 staging 行序即
            // 屏幕顶部序，读回无需翻转；与 GL 路径的翻 Y 语义对齐）
            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { (int32_t)extent_.width, (int32_t)extent_.height, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { (int32_t)extent_.width, (int32_t)extent_.height, 1 };
            vkCmdBlitImage(cmd_, images_[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           shotImg_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_NEAREST);
            // 布局回转：交换链回 PRESENT_SRC（present 需要）；staging
            // 转 GENERAL（host map 读）
            b[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            b[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            b[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            b[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            b[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                                 0, nullptr, 0, nullptr, 2, b);
        }
    }

    vkEndCommandBuffer(cmd_);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &imageAvailable_;
    si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd_;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &renderFinished_;
    vkQueueSubmit(queue_, 1, &si, fence_);

    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderFinished_;
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain_; pi.pImageIndices = &idx;
    vkQueuePresentKHR(queue_, &pi);

    // 截图读回：blit 已随本帧提交，等 fence（GPU 完成）后 map staging
    // → 按格式重排（B8G8R8A8 交换 R/B）+ 去 alpha → 公共 PNG 落盘
    if (wantShot && shotImg_ != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
        void* mapped = nullptr;
        if (vkMapMemory(device_, shotMem_, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS) {
            const uint32_t w = extent_.width, h = extent_.height;
            const bool bgra = (format_ == VK_FORMAT_B8G8R8A8_UNORM);
            std::vector<uint8_t> rgb((size_t)w * h * 3);
            const uint8_t* src = (const uint8_t*)mapped;
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* row = src + (size_t)y * w * 4;
                uint8_t* dst = rgb.data() + (size_t)y * w * 3;
                for (uint32_t x = 0; x < w; ++x) {
                    dst[x * 3 + 0] = row[bgra ? x * 4 + 2 : x * 4 + 0];
                    dst[x * 3 + 1] = row[x * 4 + 1];
                    dst[x * 3 + 2] = row[bgra ? x * 4 + 0 : x * 4 + 2];
                }
            }
            vkUnmapMemory(device_, shotMem_);
            writeScreenshotPNG(shotTag_, rgb.data(), (int)w, (int)h);
        } else {
            UI_LOG_ERROR("screenshot: staging map failed");
        }
    }

    // 本帧已提交：安全释放帧内销毁的纹理（cmds 已消费完毕）
    flushRetired();
}

// ============================================================
// 帧包回放（渲染线程执行）
//
// 1) 纹理命令按录制序执行：创建=上传像素并以包内 id 注册（不重分 id，
//    两线程 id 空间一致）；销毁=摘除映射挂 retired_ 延迟到提交后释放
//    （帧内重建图集时已记录命令仍安全——与直通同策略）；
// 2) 帧提交：几何/命令/清屏色全部来自帧包（presentImpl 参数化）。
// 与直通路径产生完全相同的 VK 调用序列 → 渲染效果逐位一致。
// ============================================================
void VKDevice::replay(const FramePacket& pkt) {
    if (!ready_) return;
    for (const auto& tc : pkt.vkTexCmds) {
        if (tc.destroy) {
            auto it = textures_.find(tc.id);
            if (it != textures_.end()) {
                retired_.push_back(it->second);
                textures_.erase(it);
            }
        } else {
            // 回放侧创建：forceId 用录制侧分配的 id 注册（失败不注册，
            // 后续绘制该 id 回退白纹理——安全降级）
            createTextureInternal(tc.w, tc.h, tc.rgba.data(), tc.id);
        }
    }
    presentImpl(pkt.verts, pkt.indices, pkt.vkCmds,
                Color(pkt.vkCr, pkt.vkCg, pkt.vkCb, pkt.vkCa));
}

void VKDevice::clear(const Color& c) { clearColor_ = c; }
void VKDevice::setViewport(int, int, int, int) {}

// ============================================================
// 窗口截图（引擎内拍照；请求-消费模式，语义同 GLDevice）
// requestScreenshot 任意线程置标记；presentImpl（直通=主线程 /
// 回放=渲染线程）renderPass 后 blit 到线性 staging，present 完
// 等 fence map 读回 → 格式重排 → 公共 PNG 落盘。
// ============================================================
void VKDevice::requestScreenshot(const char* tag) {
    if (!tag || !tag[0]) tag = "default";
    std::strncpy(shotTag_, tag, sizeof(shotTag_) - 1);
    shotTag_[sizeof(shotTag_) - 1] = 0;
    shotReq_.store(true, std::memory_order_release);
}

void VKDevice::ensureShotStaging() {
    if (!device_ || extent_.width == 0 || extent_.height == 0) return;
    // 尺寸未变且已创建：复用
    if (shotImg_ != VK_NULL_HANDLE &&
        shotExtent_.width == extent_.width && shotExtent_.height == extent_.height)
        return;
    if (shotImg_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, shotImg_, nullptr);
        vkFreeMemory(device_, shotMem_, nullptr);
        shotImg_ = VK_NULL_HANDLE; shotMem_ = VK_NULL_HANDLE;
    }
    // 线性平铺 + host-visible：map 直接读（无需 staging buffer 中转）
    VkImageCreateInfo ii{}; ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format_;
    ii.extent = { extent_.width, extent_.height, 1 };
    ii.mipLevels = 1; ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_LINEAR;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &ii, nullptr, &shotImg_) != VK_SUCCESS) {
        shotImg_ = VK_NULL_HANDLE;
        UI_LOG_ERROR("screenshot: staging image create failed");
        return;
    }
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device_, shotImg_, &req);
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(physical_, &mp);
    // HOST_VISIBLE | HOST_CACHED 优先（读回性能），回落任何 HOST_VISIBLE
    uint32_t typeIdx = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if (!(req.memoryTypeBits & (1u << i))) continue;
        if ((mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
            typeIdx = i; break;
        }
    }
    if (typeIdx == UINT32_MAX) {
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                typeIdx = i; break;
            }
        }
    }
    if (typeIdx == UINT32_MAX) {
        vkDestroyImage(device_, shotImg_, nullptr);
        shotImg_ = VK_NULL_HANDLE;
        UI_LOG_ERROR("screenshot: no host-visible memory type");
        return;
    }
    VkMemoryAllocateInfo mai{}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = typeIdx;
    if (vkAllocateMemory(device_, &mai, nullptr, &shotMem_) != VK_SUCCESS ||
        vkBindImageMemory(device_, shotImg_, shotMem_, 0) != VK_SUCCESS) {
        if (shotMem_ != VK_NULL_HANDLE) vkFreeMemory(device_, shotMem_, nullptr);
        vkDestroyImage(device_, shotImg_, nullptr);
        shotImg_ = VK_NULL_HANDLE; shotMem_ = VK_NULL_HANDLE;
        UI_LOG_ERROR("screenshot: staging alloc/bind failed");
        return;
    }
    shotExtent_ = extent_;
}

#else // no Vulkan SDK

bool VKDevice::init(void*, int, int) {
    UI_LOG_ERROR("Vulkan backend not built (no Vulkan SDK). Reconfigure with -DUIENGINE_BUILD_VULKAN=ON and install the SDK.");
    return false;
}
void VKDevice::shutdown() {}
void VKDevice::resize(int, int) {}
void VKDevice::beginFrame() {}
void VKDevice::endFrame()   {}
void VKDevice::present()    {}
void VKDevice::clear(const Color& c) { clearColor_ = c; }
void VKDevice::setViewport(int, int, int, int) {}
void VKDevice::setScissorRect(int, int, int, int) {}
void VKDevice::resetScissorRect() {}
void VKDevice::pushScissorRect(int, int, int, int) {}
void VKDevice::popScissorRect() {}
void VKDevice::setAAEnabled(bool) {}
void VKDevice::setSDFParams(int, float, float, float, float, float, float, float, float, float) {}
void VKDevice::setTargetSize(int, int) {}   // 渲染目标尺寸（stub：无 VK 资源）
void VKDevice::setRecording(FramePacket* pkt) { (void)pkt; }
void VKDevice::replay(const FramePacket& pkt) { (void)pkt; }
void VKDevice::requestScreenshot(const char* tag) { (void)tag; }
uint32_t VKDevice::createTexture(int, int, const uint8_t*) { return 0; }
void VKDevice::destroyTexture(uint32_t) {}
void VKDevice::drawTriangles(const Vertex*, int, const uint32_t*, int, uint32_t, bool) {
    if (!drawWarned_) { UI_LOG_WARN("Vulkan stub drawTriangles"); drawWarned_ = true; }
}

#endif

} // namespace ui

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#include "fast_obj.h"

#define countof(arr) sizeof(arr) / sizeof(arr[0])

#ifndef NDEBUG
#define VK_CHECK(call) \
    { \
        VkResult result_ = (call); \
        assert(result_ == VK_SUCCESS); \
    }
#else
#define VK_CHECK(call) call
#endif

VkInstance createInstance(void)
{
    static const VkApplicationInfo appInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };

    static const char* extensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif

#ifndef NDEBUG
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
    };

#ifndef NDEBUG
    static const char* layers[] =
    {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    static const VkInstanceCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .ppEnabledExtensionNames = extensions,
        .enabledExtensionCount = countof(extensions),

#ifndef NDEBUG
        .ppEnabledLayerNames = layers,
        .enabledLayerCount = countof(layers),
#endif
    };

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

#ifndef NDEBUG

VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char*                                 pLayerPrefix,
    const char*                                 pMessage,
    void*                                       pUserData
)
{
    (void) objectType, object, location, messageCode, pLayerPrefix, pUserData;

    if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        return VK_FALSE;

    const char* type =
        (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        ? "ERROR"
        : (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
            ? "WARNING"
            : "INFO";

    printf("[%s]: %s\n", type, pMessage);

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        assert(!"Validation error encountered!");

    return VK_FALSE;
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{
    const VkDebugReportCallbackCreateInfoEXT createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = debugReportCallback,
    };

    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackEXT debugCallback = 0;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &createInfo, 0, &debugCallback));

    return debugCallback;
}

#endif

VkPhysicalDevice pickPhysicalDevice(VkInstance instance)
{
    uint32_t physicalDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0));

    VkPhysicalDevice* physicalDevices = calloc(physicalDeviceCount, sizeof(*physicalDevices));
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = 0;
    if (physicalDeviceCount > 0)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[0], &props);

        printf("Picking fallback GPU: %s\n", props.deviceName);
        physicalDevice = physicalDevices[0];

        free(physicalDevices);
    }
    else
    {
        printf("No physical devices available\n");
    }

    return physicalDevice;
}

uint32_t getGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, 0);

    VkQueueFamilyProperties* queues = calloc(queueCount, sizeof(*queues));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues);

    uint32_t familyIndex = VK_QUEUE_FAMILY_IGNORED;

    for (uint32_t i = 0; i < queueCount; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            familyIndex = i;
            break;
        }

    free(queues);
    return familyIndex;
}

VkDevice createDevice(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{
    const VkDeviceQueueCreateInfo queueInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = familyIndex,
        .queueCount = 1,
        .pQueuePriorities = (float[]){1.0f},
    };

    static const char* extensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const VkDeviceCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queueInfo,
        .queueCreateInfoCount = 1,
        .ppEnabledExtensionNames = extensions,
        .enabledExtensionCount = countof(extensions),
    };

    VkDevice device = 0;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &device));

    return device;
}

VkShaderModule loadShader(VkDevice device, const char* path)
{
    FILE* file = fopen(path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    assert(len >= 0);
    fseek(file, 0, SEEK_SET);

    char* buf = calloc(len, sizeof(char));
    assert(buf);

    size_t rc = fread(buf, 1, len, file);
    if (rc != len)
        assert(!"Failed to read file");
    
    fclose(file);

    const VkShaderModuleCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (uint32_t*) buf,
        .codeSize = len,
    };

    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, 0, &shaderModule));

    free(buf);

    return shaderModule;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{
    const VkPipelineLayoutCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));

    return layout;
}

typedef struct
{
    float position[3];
    float normal[3];
    float texcoord[2];
} Vertex;

typedef struct
{
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    void*           data;
    size_t          size;
} Buffer;

uint32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties* memProps, uint32_t memTypeBits, VkMemoryPropertyFlags flags)
{
    for (uint32_t i = 0; i < memProps->memoryTypeCount; i++)
        if ((memTypeBits & (1 << i)) != 0 && (memProps->memoryTypes[i].propertyFlags & flags) == flags)
            return i;

    assert(!"No compatible memory type found!");
    return ~0u;
}

void createBuffer(Buffer* buffer, VkDevice device, const VkPhysicalDeviceMemoryProperties* memProps, size_t size, VkBufferUsageFlags usage)
{
    const VkBufferCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer->buffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &memReq);

    uint32_t memTypeIndex = selectMemoryType(memProps, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    assert(memTypeIndex != ~0u);

    const VkMemoryAllocateInfo allocateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memTypeIndex,
    };

    VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &buffer->memory));
    VK_CHECK(vkBindBufferMemory(device, buffer->buffer, buffer->memory, 0));

    VK_CHECK(vkMapMemory(device, buffer->memory, 0, size, 0, &buffer->data));

    buffer->size = size;
}

void destroyBuffer(VkDevice device, Buffer* buffer)
{
    vkFreeMemory(device, buffer->memory, 0);
    vkDestroyBuffer(device, buffer->buffer, 0);
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs, VkPipelineLayout layout)
{
    const VkPipelineShaderStageCreateInfo stages[] =
    {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vs,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fs,
            .pName = "main",
        },
    };

    const VkVertexInputBindingDescription stream =
    {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    const VkVertexInputAttributeDescription attrs[] =
    {
        {
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
        {
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, texcoord),
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &stream,
        .vertexAttributeDescriptionCount = countof(attrs),
        .pVertexAttributeDescriptions = attrs,
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    const VkPipelineViewportStateCreateInfo viewportState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampleState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    const VkPipelineColorBlendAttachmentState colorAttachmentState =
    {
        .colorWriteMask =   VK_COLOR_COMPONENT_R_BIT |
                            VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachmentState,
    };

    const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    const VkPipelineDynamicStateCreateInfo dynamicState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = countof(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    const VkGraphicsPipelineCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = countof(stages),
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = layout,
        .renderPass = renderPass,
    };

    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

    return pipeline;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
    const VkWin32SurfaceCreateInfoKHR createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandleA(0),
        .hwnd = glfwGetWin32Window(window),
    };

    VkSurfaceKHR surface = 0;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, 0, &surface));

    return surface;
}

VkFormat getSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));

    VkSurfaceFormatKHR* formats = calloc(formatCount, sizeof(*formats));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats));

    VkFormat format = formats[0].format;
    free(formats);
    
    return format;
}

VkRenderPass createRenderPass(VkDevice device, VkFormat format)
{
    const VkAttachmentDescription attachments[] =
    {
        {
            .format = format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };

    const VkAttachmentReference colorAttachment =
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    const VkSubpassDescription subpass =
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };

    const VkRenderPassCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = countof(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    VkRenderPass renderPass = 0;
    vkCreateRenderPass(device, &createInfo, 0, &renderPass);

    return renderPass;
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format)
{
    const VkImageViewCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.levelCount = 1,
        .subresourceRange.layerCount = 1,
    };

    VkImageView imageView = 0;
    VK_CHECK(vkCreateImageView(device, &createInfo, 0, &imageView));

    return imageView;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height)
{
    const VkFramebufferCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = 1,
        .pAttachments = &imageView,
        .width = width,
        .height = height,
        .layers = 1,
    };

    VkFramebuffer framebuffer = 0;
    VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

    return framebuffer;
}

typedef struct
{
    VkSwapchainKHR  swapchain;

    uint32_t        width, height;
    uint32_t        imageCount;

    VkImage*        images;
    VkImageView*    imageViews;
    VkFramebuffer*  framebuffers;
} Swapchain;

void createSwapchain(Swapchain* swapchain, VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, uint32_t width, uint32_t height, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain)
{
    const VkSwapchainCreateInfoKHR createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = 2,
        .imageFormat = format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent.width = width,
        .imageExtent.height = height,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &familyIndex,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .oldSwapchain = oldSwapchain,
    };

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain->swapchain));
    assert(swapchain->swapchain);

    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->imageCount, 0));

    swapchain->images = calloc(swapchain->imageCount, sizeof(*swapchain->images));
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchain->imageCount, swapchain->images));

    swapchain->imageViews = calloc(swapchain->imageCount, sizeof(*swapchain->imageViews));
    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        swapchain->imageViews[i] = createImageView(device, swapchain->images[i], format);
        assert(swapchain->imageViews[i]);
    }

    swapchain->framebuffers = calloc(swapchain->imageCount, sizeof(*swapchain->framebuffers));
    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        swapchain->framebuffers[i] = createFramebuffer(device, renderPass, swapchain->imageViews[i], width, height);
        assert(swapchain->framebuffers[i]);
    }

    swapchain->width = width;
    swapchain->height = height;
}

void destroySwapchain(VkDevice device, Swapchain* swapchain)
{
    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        vkDestroyFramebuffer(device, swapchain->framebuffers[i], 0);
        vkDestroyImageView(device, swapchain->imageViews[i], 0);
    }
    free(swapchain->framebuffers);
    free(swapchain->imageViews);
    free(swapchain->images);

    vkDestroySwapchainKHR(device, swapchain->swapchain, 0);
}

void resizeSwapchainIfNecessary(Swapchain* swapchain, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, VkRenderPass renderPass)
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    uint32_t newWidth = surfaceCaps.currentExtent.width;
    uint32_t newHeight = surfaceCaps.currentExtent.height;

    if (swapchain->width == newWidth && swapchain->height == newHeight)
        return;

    Swapchain old = *swapchain;

    createSwapchain(swapchain, device, surface, familyIndex, format, newWidth, newHeight, renderPass, old.swapchain);

    VK_CHECK(vkDeviceWaitIdle(device));

    destroySwapchain(device, &old);
}

VkSemaphore createSemaphore(VkDevice device)
{
    const VkSemaphoreCreateInfo createInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore = 0;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

    return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex)
{
    const VkCommandPoolCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = familyIndex,
    };

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

    return commandPool;
}

Vertex* loadObj(const char* path, size_t* pSize)
{
    fastObjMesh* obj = fast_obj_read(path);
    if (!obj)
        return 0;

    size_t index_count = 0;

    for (uint32_t i = 0; i < obj->face_count; i++)
        index_count += 3 * (obj->face_vertices[i] - 2);

    Vertex* vertices = calloc(index_count, sizeof(*vertices));
    *pSize = index_count * sizeof(Vertex);

    size_t vertex_offset = 0;
    size_t index_offset = 0;

    for (uint32_t i = 0; i < obj->face_count; i++)
    {
        for (uint32_t j = 0; j < obj->face_vertices[i]; j++)
        {
            fastObjIndex gi = obj->indices[index_offset + j];

            // triangulate polygons on the fly
            if (j >= 3)
            {
                vertices[vertex_offset + 0] = vertices[vertex_offset - 3];
                vertices[vertex_offset + 1] = vertices[vertex_offset - 1];
                vertex_offset += 2;
            }

            Vertex* v = &vertices[vertex_offset++];

            v->position[0] = obj->positions[gi.p * 3 + 0];
            v->position[1] = obj->positions[gi.p * 3 + 1];
            v->position[2] = obj->positions[gi.p * 3 + 2];

            v->normal[0] = obj->normals[gi.n * 3 + 0];
            v->normal[1] = obj->normals[gi.n * 3 + 1];
            v->normal[2] = obj->normals[gi.n * 3 + 2];

            v->texcoord[0] = obj->texcoords[gi.t * 2 + 0];
            v->texcoord[1] = obj->texcoords[gi.t * 2 + 1];
        }

        index_offset += obj->face_vertices[i];
    }

    assert(vertex_offset == index_offset);

    fast_obj_destroy(obj);

    return vertices;
}

int main(int argc, char* argv[])
{
    (void) argc, argv;

    int rc = glfwInit();
    if (rc == 0)
        return 1;

    VkInstance instance = createInstance();
    assert(instance);

#ifndef NDEBUG
    VkDebugReportCallbackEXT debugCallback = registerDebugCallback(instance);
    assert(debugCallback);
#endif

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance);
    assert(physicalDevice);

    uint32_t familyIndex = getGraphicsQueueFamily(physicalDevice);
    assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);

    VkDevice device = createDevice(physicalDevice, familyIndex);
    assert(device);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Vulkan Renderer", 0, 0);
    assert(window);

    VkSurfaceKHR surface = createSurface(instance, window);
    assert(surface);

    VkFormat surfaceFormat = getSurfaceFormat(physicalDevice, surface);

    VkRenderPass renderPass = createRenderPass(device, surfaceFormat);
    assert(renderPass);

    VkShaderModule triangleVS = loadShader(device, "bin/trig.vert.spv");
    assert(triangleVS);
    
    VkShaderModule triangleFS = loadShader(device, "bin/trig.frag.spv");
    assert(triangleFS);

    // TODO: this is critical for performance!
    VkPipelineCache pipelineCache = 0;

    VkPipelineLayout triangleLayout = createPipelineLayout(device);
    assert(triangleLayout);

    VkPipeline trianglePipeline = createGraphicsPipeline(device, pipelineCache, renderPass, triangleVS, triangleFS, triangleLayout);
    assert(trianglePipeline);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    Swapchain swapchain;
    createSwapchain(&swapchain, device, surface, familyIndex, surfaceFormat, windowWidth, windowHeight, renderPass, 0);

    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    const VkCommandBufferAllocateInfo allocateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    VkCommandBuffer commandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    size_t vertices_size = 0;
    Vertex* vertices = loadObj("data/kitten.obj", &vertices_size);

    size_t vertex_count = vertices_size / sizeof(Vertex);

    Buffer vb = {0};
    createBuffer(&vb, device, &memProps, 128 * 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Buffer ib = {0};
    // createBuffer(&ib, device, &memProps, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    memcpy(vb.data, vertices, vertices_size);

    glfwShowWindow(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        while (windowWidth == 0 || windowHeight == 0)
        {
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            glfwWaitEvents();
        }

        resizeSwapchainIfNecessary(&swapchain, physicalDevice, device, surface, familyIndex, surfaceFormat, renderPass);

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain, ~0ull, acquireSemaphore, 0, &imageIndex));
        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        const VkCommandBufferBeginInfo beginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        const VkClearColorValue color = { 48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1 };
        const VkClearValue clearColor = { color };

        const VkRenderPassBeginInfo passBeginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = swapchain.framebuffers[imageIndex],
            .renderArea.extent.width = swapchain.width,
            .renderArea.extent.height = swapchain.height,
            .clearValueCount = 1,
            .pClearValues = &clearColor,
        };

        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = { 0, 0, (float) swapchain.width, (float) swapchain.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {swapchain.width, swapchain.height} };

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkDeviceSize dummyOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb.buffer, &dummyOffset);
        /* vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, countof(indices), 1, 0, 0, 0); */

        vkCmdDraw(commandBuffer, (uint32_t) vertex_count, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        const VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        const VkSubmitInfo submitInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &acquireSemaphore,
            .pWaitDstStageMask = &submitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &releaseSemaphore,
        };

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));

        const VkPresentInfoKHR presentInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &releaseSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &imageIndex,
        };

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));
    }

    /* destroyBuffer(device, &ib); */
    destroyBuffer(device, &vb);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, 0);

    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);

    destroySwapchain(device, &swapchain);

    vkDestroyPipeline(device, trianglePipeline, 0);
    vkDestroyPipelineLayout(device, triangleLayout, 0);
    vkDestroyShaderModule(device, triangleFS, 0);
    vkDestroyShaderModule(device, triangleVS, 0);

    vkDestroyRenderPass(device, renderPass, 0);
    vkDestroySurfaceKHR(instance, surface, 0);

    glfwDestroyWindow(window);

    vkDestroyDevice(device, 0);

#ifndef NDEBUG
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

    vkDestroyDebugReportCallbackEXT(instance, debugCallback, 0);
#endif

    vkDestroyInstance(instance, 0);

    glfwTerminate();

    return 0;
}

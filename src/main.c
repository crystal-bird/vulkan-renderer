
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define countof(arr) sizeof(arr) / sizeof(arr[0])

#ifdef _DEBUG
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

VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, uint32_t width, uint32_t height)
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
    };

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

    return swapchain;
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

VkImage* getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount)
{
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, 0));
    
    VkImage* swapchainImages = calloc(*pSwapchainImageCount, sizeof(*swapchainImages));
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, swapchainImages));

    return swapchainImages;
}

int main(int argc, char* argv[])
{
    (void) argc, argv;

    int rc = glfwInit();
    if (rc == 0)
        return 1;

    VkInstance instance = createInstance();
    assert(instance);

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

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    VkFormat surfaceFormat = getSurfaceFormat(physicalDevice, surface);

    VkSwapchainKHR swapchain = createSwapchain(device, surface, familyIndex, surfaceFormat, windowWidth, windowHeight);
    assert(swapchain);

    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    uint32_t swapchainImageCount = 0;
    VkImage* swapchainImages = getSwapchainImages(device, swapchain, &swapchainImageCount);
    assert(swapchainImages);

    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    glfwShowWindow(window);

    const VkCommandBufferAllocateInfo allocateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    VkCommandBuffer commandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, ~0ull, acquireSemaphore, 0, &imageIndex));
        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        const VkCommandBufferBeginInfo beginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        const VkClearColorValue color = { 1, 0, 1, 1 };

        const VkImageSubresourceRange range = 
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };

        vkCmdClearColorImage(commandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

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
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
        };

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, 0);

    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);

    vkDestroySwapchainKHR(device, swapchain, 0);
    vkDestroySurfaceKHR(instance, surface, 0);

    glfwDestroyWindow(window);

    vkDestroyDevice(device, 0);
    vkDestroyInstance(instance, 0);

    glfwTerminate();

    return 0;
}

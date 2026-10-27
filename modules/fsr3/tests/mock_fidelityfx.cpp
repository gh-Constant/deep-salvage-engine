// TEST ONLY. This shared library exercises loading/version selection and native
// command recording. Its green output is deliberately NOT an FSR implementation.
#include "thirdparty/amd-fidelityfx-api/vk/ffx_api_vk.h"
#include "thirdparty/amd-fidelityfx-api/ffx_upscale.h"
#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
struct Context {
    VkDevice device{};
    VkPipeline pipeline{};
    VkPipelineLayout layout{};
    VkDescriptorSetLayout set_layout{};
    VkDescriptorPool pool{};
    VkDescriptorSet sets[16]{};
    uint32_t index=0;
    std::map<VkImage,VkImageView> views;
};
static void trace(const char *event) {
    if(const char *path=std::getenv("FSR3_MOCK_TRACE")) if(FILE *f=std::fopen(path,"a")) {std::fprintf(f,"%s\n",event);std::fclose(f);}
}
extern "C" ffxReturnCode_t ffxCreateContext(ffxContext *out,ffxCreateContextDescHeader *header,const ffxAllocationCallbacks *) {
    auto *create=reinterpret_cast<ffxCreateContextDescUpscale *>(header);
    auto *backend=reinterpret_cast<ffxCreateBackendVKDesc *>(header->pNext);
    if(!out||header->type!=FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE||backend->header.type!=FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK) return FFX_API_RETURN_ERROR_PARAMETER;
    if(!backend->header.pNext || backend->header.pNext->type!=FFX_API_DESC_TYPE_OVERRIDE_VERSION || reinterpret_cast<ffxOverrideVersion *>(backend->header.pNext)->versionId!=0x55) return FFX_API_RETURN_ERROR_PARAMETER;
    auto *ctx=new Context;ctx->device=backend->vkDevice;
    std::ifstream file(std::getenv("FSR3_MOCK_SPIRV"),std::ios::binary|std::ios::ate);
    if(!file) {delete ctx;return FFX_API_RETURN_ERROR;}
    size_t bytes=file.tellg();file.seekg(0);std::vector<uint32_t> words((bytes+3)/4);file.read(reinterpret_cast<char *>(words.data()),bytes);
    VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader_info.codeSize=bytes;shader_info.pCode=words.data();
    VkShaderModule shader{};if(vkCreateShaderModule(ctx->device,&shader_info,nullptr,&shader)!=VK_SUCCESS){delete ctx;return FFX_API_RETURN_ERROR;}
    VkDescriptorSetLayoutBinding binding{};binding.binding=0;binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;binding.descriptorCount=1;binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};set_info.bindingCount=1;set_info.pBindings=&binding;
    vkCreateDescriptorSetLayout(ctx->device,&set_info,nullptr,&ctx->set_layout);
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout_info.setLayoutCount=1;layout_info.pSetLayouts=&ctx->set_layout;
    vkCreatePipelineLayout(ctx->device,&layout_info,nullptr,&ctx->layout);
    VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline_info.layout=ctx->layout;
    pipeline_info.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pipeline_info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline_info.stage.module=shader;pipeline_info.stage.pName="main";
    VkResult result=vkCreateComputePipelines(ctx->device,VK_NULL_HANDLE,1,&pipeline_info,nullptr,&ctx->pipeline);
    vkDestroyShaderModule(ctx->device,shader,nullptr);
    if(result!=VK_SUCCESS) return FFX_API_RETURN_ERROR;
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,16};
    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool_info.maxSets=16;pool_info.poolSizeCount=1;pool_info.pPoolSizes=&size;
    vkCreateDescriptorPool(ctx->device,&pool_info,nullptr,&ctx->pool);
    VkDescriptorSetLayout layouts[16];for(auto &layout:layouts) layout=ctx->set_layout;
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocate.descriptorPool=ctx->pool;allocate.descriptorSetCount=16;allocate.pSetLayouts=layouts;
    vkAllocateDescriptorSets(ctx->device,&allocate,ctx->sets);
    *out=ctx;trace("CREATE");return FFX_API_RETURN_OK;
}
extern "C" ffxReturnCode_t ffxDestroyContext(ffxContext *context,const ffxAllocationCallbacks *) {
    auto *ctx=static_cast<Context *>(*context);
    for(auto entry:ctx->views) vkDestroyImageView(ctx->device,entry.second,nullptr);
    vkDestroyDescriptorPool(ctx->device,ctx->pool,nullptr);vkDestroyPipeline(ctx->device,ctx->pipeline,nullptr);
    vkDestroyPipelineLayout(ctx->device,ctx->layout,nullptr);vkDestroyDescriptorSetLayout(ctx->device,ctx->set_layout,nullptr);
    delete ctx;*context=nullptr;trace("DESTROY");return FFX_API_RETURN_OK;
}
extern "C" ffxReturnCode_t ffxConfigure(ffxContext *,const ffxConfigureDescHeader *) { return FFX_API_RETURN_OK; }
extern "C" ffxReturnCode_t ffxQuery(ffxContext *,ffxQueryDescHeader *header) {
    if(header->type==FFX_API_QUERY_DESC_TYPE_GET_VERSIONS) {
        auto *query=reinterpret_cast<ffxQueryDescGetVersions *>(header);
        if(*query->outputCount && query->versionIds) query->versionIds[0]=0x55;
        if(*query->outputCount && query->versionNames) query->versionNames[0]="TEST_GREEN_COMPUTE_ONLY";
        *query->outputCount=1;return FFX_API_RETURN_OK;
    }
    if(header->type==FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION) {
        auto *query=reinterpret_cast<ffxQueryGetProviderVersion *>(header);query->versionId=0x55;query->versionName="TEST_GREEN_COMPUTE_ONLY";return FFX_API_RETURN_OK;
    }
    return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
}
extern "C" ffxReturnCode_t ffxDispatch(ffxContext *context,const ffxDispatchDescHeader *header) {
    auto *ctx=static_cast<Context *>(*context);auto *desc=reinterpret_cast<const ffxDispatchDescUpscale *>(header);
    VkImage image=reinterpret_cast<VkImage>(desc->output.resource);
    if(!ctx->views.count(image)) {
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};info.image=image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=VK_FORMAT_R16G16B16A16_SFLOAT;
        info.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;info.subresourceRange.levelCount=1;info.subresourceRange.layerCount=1;
        if(vkCreateImageView(ctx->device,&info,nullptr,&ctx->views[image])!=VK_SUCCESS) return FFX_API_RETURN_ERROR;
    }
    VkDescriptorSet set=ctx->sets[(ctx->index++)%16];VkDescriptorImageInfo image_info{};image_info.imageView=ctx->views[image];image_info.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;write.pImageInfo=&image_info;
    vkUpdateDescriptorSets(ctx->device,1,&write,0,nullptr);
    VkCommandBuffer command=reinterpret_cast<VkCommandBuffer>(desc->commandList);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,ctx->pipeline);
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,ctx->layout,0,1,&set,0,nullptr);
    vkCmdDispatch(command,(desc->upscaleSize.width+7)/8,(desc->upscaleSize.height+7)/8,1);
    trace("DISPATCH");return FFX_API_RETURN_OK;
}

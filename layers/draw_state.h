/*
 * Vulkan
 *
 * Copyright (C) 2014 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "vkLayer.h"
#include <vector>

using namespace std;

// Draw State ERROR codes
typedef enum _DRAW_STATE_ERROR
{
    DRAWSTATE_NONE,                             // Used for INFO & other non-error messages
    DRAWSTATE_INTERNAL_ERROR,                   // Error with DrawState internal data structures
    DRAWSTATE_NO_PIPELINE_BOUND,                // Unable to identify a bound pipeline
    DRAWSTATE_INVALID_POOL,                     // Invalid DS pool
    DRAWSTATE_INVALID_SET,                      // Invalid DS
    DRAWSTATE_INVALID_LAYOUT,                   // Invalid DS layout
    DRAWSTATE_DS_END_WITHOUT_BEGIN,             // EndDSUpdate called w/o corresponding BeginDSUpdate
    DRAWSTATE_UPDATE_WITHOUT_BEGIN,             // Attempt to update descriptors w/o calling BeginDescriptorPoolUpdate
    DRAWSTATE_INVALID_PIPELINE,                 // Invalid Pipeline referenced
    DRAWSTATE_INVALID_CMD_BUFFER,               // Invalid CmdBuffer referenced
    DRAWSTATE_VTX_INDEX_OUT_OF_BOUNDS,          // binding in vkCmdBindVertexData() too large for PSO's pVertexBindingDescriptions array
    DRAWSTATE_INVALID_DYNAMIC_STATE_OBJECT,     // Invalid dyn state object
    DRAWSTATE_MISSING_DOT_PROGRAM,              // No "dot" program in order to generate png image
    DRAWSTATE_BINDING_DS_NO_END_UPDATE,         // DS bound to CmdBuffer w/o call to vkEndDescriptorSetUpdate())
    DRAWSTATE_NO_DS_POOL,                       // No DS Pool is available
    DRAWSTATE_OUT_OF_MEMORY,                    // malloc failed
    DRAWSTATE_DESCRIPTOR_TYPE_MISMATCH,         // Type in layout vs. update are not the same
    DRAWSTATE_DESCRIPTOR_UPDATE_OUT_OF_BOUNDS,  // Descriptors set for update out of bounds for corresponding layout section
    DRAWSTATE_INVALID_UPDATE_INDEX,             // Index of requested update is invalid for specified descriptors set
    DRAWSTATE_INVALID_UPDATE_STRUCT,            // Struct in DS Update tree is of invalid type
    DRAWSTATE_NUM_SAMPLES_MISMATCH,             // Number of samples in bound PSO does not match number in FB of current RenderPass
    DRAWSTATE_NO_END_CMD_BUFFER,                // Must call vkEndCommandBuffer() before QueueSubmit on that cmdBuffer
} DRAW_STATE_ERROR;

typedef enum _DRAW_TYPE
{
    DRAW                  = 0,
    DRAW_INDEXED          = 1,
    DRAW_INDIRECT         = 2,
    DRAW_INDEXED_INDIRECT = 3,
    DRAW_BEGIN_RANGE      = DRAW,
    DRAW_END_RANGE        = DRAW_INDEXED_INDIRECT,
    NUM_DRAW_TYPES        = (DRAW_END_RANGE - DRAW_BEGIN_RANGE + 1),
} DRAW_TYPE;

typedef struct _SHADER_DS_MAPPING {
    uint32_t slotCount;
    VkDescriptorSetLayoutCreateInfo* pShaderMappingSlot;
} SHADER_DS_MAPPING;

typedef struct _GENERIC_HEADER {
    VkStructureType sType;
    const void*    pNext;
} GENERIC_HEADER;

typedef struct _PIPELINE_NODE {
    VkPipeline           pipeline;

    VkGraphicsPipelineCreateInfo     graphicsPipelineCI;
    VkPipelineVertexInputCreateInfo vertexInputCI;
    VkPipelineIaStateCreateInfo     iaStateCI;
    VkPipelineTessStateCreateInfo   tessStateCI;
    VkPipelineVpStateCreateInfo     vpStateCI;
    VkPipelineRsStateCreateInfo     rsStateCI;
    VkPipelineMsStateCreateInfo     msStateCI;
    VkPipelineCbStateCreateInfo     cbStateCI;
    VkPipelineDsStateCreateInfo     dsStateCI;
    VkPipelineShaderStageCreateInfo vsCI;
    VkPipelineShaderStageCreateInfo tcsCI;
    VkPipelineShaderStageCreateInfo tesCI;
    VkPipelineShaderStageCreateInfo gsCI;
    VkPipelineShaderStageCreateInfo fsCI;
    // Compute shader is include in VkComputePipelineCreateInfo
    VkComputePipelineCreateInfo      computePipelineCI;

    VkGraphicsPipelineCreateInfo*      pCreateTree; // Ptr to shadow of data in create tree
    // Vtx input info (if any)
    uint32_t                                vtxBindingCount;   // number of bindings
    VkVertexInputBindingDescription*   pVertexBindingDescriptions;
    uint32_t                                vtxAttributeCount; // number of attributes
    VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    uint32_t                                attachmentCount;   // number of CB attachments
    VkPipelineCbAttachmentState*       pAttachments;
} PIPELINE_NODE;

typedef struct _SAMPLER_NODE {
    VkSampler              sampler;
    VkSamplerCreateInfo  createInfo;
} SAMPLER_NODE;

typedef struct _IMAGE_NODE {
    VkImageView             image;
    VkImageViewCreateInfo createInfo;
    VkImageViewAttachInfo attachInfo;
} IMAGE_NODE;

typedef struct _BUFFER_NODE {
    VkBufferView             buffer;
    VkBufferViewCreateInfo createInfo;
    VkBufferViewAttachInfo attachInfo;
} BUFFER_NODE;

typedef struct _DYNAMIC_STATE_NODE {
    VkDynamicStateObject    stateObj;
    GENERIC_HEADER*             pCreateInfo;
    union {
        VkDynamicVpStateCreateInfo vpci;
        VkDynamicRsStateCreateInfo rsci;
        VkDynamicCbStateCreateInfo cbci;
        VkDynamicDsStateCreateInfo dsci;
    } create_info;
} DYNAMIC_STATE_NODE;
// Descriptor Data structures
// Layout Node has the core layout data
typedef struct _LAYOUT_NODE {
    VkDescriptorSetLayout                    layout;
    VkDescriptorType*                         pTypes; // Dynamic array that will be created to verify descriptor types
    VkDescriptorSetLayoutCreateInfo        createInfo;
    uint32_t                                     startIndex; // 1st index of this layout
    uint32_t                                     endIndex; // last index of this layout
} LAYOUT_NODE;
typedef struct _SET_NODE {
    VkDescriptorSet                           set;
    VkDescriptorPool                          pool;
    VkDescriptorSetUsage                     setUsage;
    // Head of LL of all Update structs for this set
    GENERIC_HEADER*                              pUpdateStructs;
    // Total num of descriptors in this set (count of its layout plus all prior layouts)
    uint32_t                                     descriptorCount;
    GENERIC_HEADER**                             ppDescriptors; // Array where each index points to update node for its slot
    LAYOUT_NODE*                                 pLayout; // Layout for this set
    struct _SET_NODE*                            pNext;
} SET_NODE;

typedef struct _POOL_NODE {
    VkDescriptorPool                          pool;
    VkDescriptorPoolUsage                    poolUsage;
    uint32_t                                     maxSets;
    VkDescriptorPoolCreateInfo              createInfo;
    bool32_t                                     updateActive; // Track if Pool is in an update block
    SET_NODE*                                    pSets; // Head of LL of sets for this Pool
} POOL_NODE;

// Cmd Buffer Tracking
typedef enum _CMD_TYPE
{
    CMD_BINDPIPELINE,
    CMD_BINDPIPELINEDELTA,
    CMD_BINDDYNAMICSTATEOBJECT,
    CMD_BINDDESCRIPTORSETS,
    CMD_BINDINDEXBUFFER,
    CMD_BINDVERTEXBUFFER,
    CMD_DRAW,
    CMD_DRAWINDEXED,
    CMD_DRAWINDIRECT,
    CMD_DRAWINDEXEDINDIRECT,
    CMD_DISPATCH,
    CMD_DISPATCHINDIRECT,
    CMD_COPYBUFFER,
    CMD_COPYIMAGE,
    CMD_BLITIMAGE,
    CMD_COPYBUFFERTOIMAGE,
    CMD_COPYIMAGETOBUFFER,
    CMD_CLONEIMAGEDATA,
    CMD_UPDATEBUFFER,
    CMD_FILLBUFFER,
    CMD_CLEARCOLORIMAGE,
    CMD_CLEARCOLORIMAGERAW,
    CMD_CLEARDEPTHSTENCIL,
    CMD_RESOLVEIMAGE,
    CMD_SETEVENT,
    CMD_RESETEVENT,
    CMD_WAITEVENTS,
    CMD_PIPELINEBARRIER,
    CMD_BEGINQUERY,
    CMD_ENDQUERY,
    CMD_RESETQUERYPOOL,
    CMD_WRITETIMESTAMP,
    CMD_INITATOMICCOUNTERS,
    CMD_LOADATOMICCOUNTERS,
    CMD_SAVEATOMICCOUNTERS,
    CMD_BEGINRENDERPASS,
    CMD_ENDRENDERPASS,
    CMD_DBGMARKERBEGIN,
    CMD_DBGMARKEREND,
} CMD_TYPE;
// Data structure for holding sequence of cmds in cmd buffer
typedef struct _CMD_NODE {
    CMD_TYPE          type;
    uint64_t          cmdNumber;
} CMD_NODE;

typedef enum _CB_STATE
{
    CB_NEW,            // Newly created CB w/o any cmds
    CB_UPDATE_ACTIVE,  // BeginCB has been called on this CB
    CB_UPDATE_COMPLETE // EndCB has been called on this CB
} CB_STATE;
// Cmd Buffer Wrapper Struct
typedef struct _GLOBAL_CB_NODE {
    VkCmdBuffer                  cmdBuffer;
    uint32_t                     queueNodeIndex;
    VkFlags                      flags;
    VkFence                      fence;    // fence tracking this cmd buffer
    uint64_t                     numCmds;  // number of cmds in this CB
    uint64_t                     drawCount[NUM_DRAW_TYPES]; // Count of each type of draw in this CB
    CB_STATE                     state; // Track if cmd buffer update status
    vector<CMD_NODE*>            pCmds;
    // Currently storing "lastBound" objects on per-CB basis
    //  long-term may want to create caches of "lastBound" states and could have
    //  each individual CMD_NODE referencing its own "lastBound" state
    VkPipeline                   lastBoundPipeline;
    uint32_t                     lastVtxBinding;
    DYNAMIC_STATE_NODE*          lastBoundDynamicState[VK_NUM_STATE_BIND_POINT];
    VkDescriptorSet              lastBoundDescriptorSet;
    VkRenderPass                 activeRenderPass;
    VkFramebuffer                framebuffer;
    vector<VkDescriptorSet>      boundDescriptorSets;
} GLOBAL_CB_NODE;

//prototypes for extension functions
void drawStateDumpDotFile(char* outFileName);
void drawStateDumpPngFile(char* outFileName);
void drawStateDumpCommandBufferDotFile(char* outFileName);
// Func ptr typedefs
typedef void (*DRAW_STATE_DUMP_DOT_FILE)(char*);
typedef void (*DRAW_STATE_DUMP_PNG_FILE)(char*);
typedef void (*DRAW_STATE_DUMP_COMMAND_BUFFER_DOT_FILE)(char*);

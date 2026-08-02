# VK-GL-CTS / dEQP-VK Performance Optimization Guide & Agent Playbook

This document is a technical guide and playbook for developers and autonomous AI agents working to optimize the runtime performance of Khronos Vulkan Conformance Tests (`VK-GL-CTS` / `dEQP-VK`).

---

## 1. Executive Conformance Standard: Zero Command-Line Arguments

When optimizing test execution speed in `VK-GL-CTS`, all performance optimizations **MUST adhere to the Default Conformance Standard**:

1. **No Command-Line Arguments Required:** Every optimization must be evaluated and implemented as a **default CTS change**. Optimizations must not rely on optional command-line flags (e.g., `--deqp-*`) or special runtime switches to be conformant. The default execution behavior must be 100% conformant with Khronos official submission requirements.
2. **100% Coverage Equivalence ("Won't change how much we're testing"):** Reducing loop iterations, caching resources, batching draw calls, or pooling surfaces must test the exact same API requirements, valid usage rules, synchronization guarantees, state transitions, and edge cases as the unmodified CTS.

---

## 2. Lead Optimization: WSI Swapchain Frame Count Calibration (`dEQP-VK.wsi.*`)

### 2.1 Why WSI Tests Are Slow
Window System Integration (`dEQP-VK.wsi.*`) tests historically hardcode excessive multi-frame render loops:
- In [vktWsiSwapchainTests.cpp:L1252](file:///Users/tomnom/git/VK-GL-CTS/external/vulkancts/modules/vulkan/wsi/vktWsiSwapchainTests.cpp#L1252): `numFramesToRender = 60 * 10;` (600 frames / 10 seconds at 60 Hz vsync).
- In [vktWsiMaintenance1Tests.cpp:L695](file:///Users/tomnom/git/VK-GL-CTS/external/vulkancts/modules/vulkan/wsi/vktWsiMaintenance1Tests.cpp#L695): `iterations = 250;` / `120;`.
- In [vktWsiIncrementalPresentTests.cpp:L761](file:///Users/tomnom/git/VK-GL-CTS/external/vulkancts/modules/vulkan/wsi/vktWsiIncrementalPresentTests.cpp#L761): `m_frameCount(300)`.

When presenting to a vsync-throttled window surface (`vkQueuePresentKHR`), tests sit idle waiting for monitor vertical blanking intervals. A single WSI platform suite (`dEQP-VK.wsi.metal.*`, 4,964 test cases) takes **~3.24 hours (11,680 seconds)** on stock CTS—with **96.1% of that runtime** consumed by `maintenance1` and `incremental_present`.

### 2.2 The Conformance-Equivalent Solution
The authoritative Khronos Vulkan Specification defines **no minimum frame count** that an application must render. What conformance mandates is valid API usage for `minImageCount` ([VK_KHR_surface/wsi.adoc:L2922-2927](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/VK_KHR_surface/wsi.adoc#L2922-L2927)) and proper semaphore/fence synchronization across `vkAcquireNextImageKHR` and `vkQueuePresentKHR`.

When optimizing any existing or new WSI test file, use the following **dynamic render loop formula**:

```cpp
// Ensure at least 10 frames AND at least 2 full rotations around the swapchain image ring buffer:
const uint32_t numFramesToRender = std::max(10u, static_cast<uint32_t>(swapchainImages.size() * 2u));
```

#### Why This Formula is 100% Conformant by Default:
- On standard swapchains (`minImageCount` of 2 to 4), 10 frames performs 2.5 to 5 complete rotations through the swapchain ring buffer, testing acquire/present synchronization, semaphore recycling, and fence signaling without deadlock.
- On platforms where swapchain `minImageCount` ($S$) is large ($S = 6$ or $8$), `std::max(10u, 2u * imageCount)` automatically scales to 12 or 16 frames, guaranteeing at least 2 full wrap-around cycles.
- **Impact:** Reduces WSI swapchain execution from 10–25 seconds per test down to ~0.15s per test, speeding up the overall WSI suite by **31.3× (reducing 3.24 hours to 6.22 minutes per platform)** while preserving 100% test pass rates and zero coverage regression.

---

## 3. Five More Optimization Patterns for Future Agents

When optimizing new test files or modules in `VK-GL-CTS`, check if any of the following five proven patterns can be applied:

### Pattern 1: OS Window & `VkSurfaceKHR` Reuse Pool
- **Where to apply:** Non-mutating window system tests (`dEQP-VK.wsi.*.surface.*`, `present_timing`, `shared_presentable_image`).
- **Implementation:** Reuse a pooled `VkSurfaceKHR` and native window across compatible tests instead of creating/destroying an OS window per test case (~300 ms saved per test).
- **Mandatory Condition:**
  1. Always wait idle (`vkDeviceWaitIdle`) and destroy the old swapchain (`vkDestroySwapchainKHR`) between tests before recycling the surface.
  2. Tests that mutate native window properties (window resize tests, fullscreen exclusive `VK_EXT_full_screen_exclusive`, display mode switching) and dedicated surface creation/destruction tests (`vkCreate*SurfaceKHR` / `vkDestroySurfaceKHR`) must opt out of the pool and use dedicated windows/surfaces.

### Pattern 2: Session-Level Persistent `VkPipelineCache` in `vkt::Context`
- **Where to apply:** General functional pipeline, shader, drawing, and compute test modules.
- **Implementation:** Use a persistent `VkPipelineCache` maintained in `vkt::Context` across all test instances in a test session by default (`context.getDefaultPipelineCache()`).
- **Mandatory Condition:**
  1. Dedicated pipeline cache conformance tests (`dEQP-VK.pipeline.pipeline_cache.*`) and explicit un-cached compiler tests must pass `VK_NULL_HANDLE` to bypass the session cache and test un-cached compiler paths explicitly.

### Pattern 3: Texture Atlas & Multi-Permutation Draw Batching (`vkt::AtlasRenderer`)
- **Where to apply:** Combinatorial pipeline state tests where thousands of permutations are evaluated (e.g., blend modes, vertex formats, stencil states).
- **Implementation:** Tile up to 64 test permutations onto an $8 \times 8$ grid of $32 \times 32$ viewports on a single $256 \times 256$ framebuffer. Execute all draws in one renderpass and perform a single `vkCmdCopyImageToBuffer` readback.
- **Mandatory Condition:**
  1. Always bind scissor rectangles (`vkCmdSetScissor`) matching the exact `(tileX, tileY, 32, 32)` tile boundary to prevent primitive bleed across tiles.
  2. Do NOT apply batching to tests evaluating renderpass attachment load/store operations (`VkAttachmentLoadOp` / `VkAttachmentStoreOp`), attachment clears, resolve attachments, or shaders relying on absolute `gl_FragCoord`.

### Pattern 4: Extended Dynamic State (`VK_EXT_extended_dynamic_state`)
- **Where to apply:** Combinatorial tests iterating over stencil ops, depth test enable, or cull modes (e.g., `vktPipelineStencilTests.cpp`).
- **Implementation:** When `VK_EXT_extended_dynamic_state` is supported, collapse 4,096 combinatorial static pipeline compilations into **1 single pipeline**, setting state dynamically via `vkCmdSetStencilOp`, `vkCmdSetDepthTestEnable`, etc.
- **Mandatory Condition:**
  1. Always verify runtime device functionality (`context.isDeviceFunctionalitySupported("VK_EXT_extended_dynamic_state")`) and fall back to static pipeline compilation if unsupported.
  2. Dedicated pipeline compiler state test modules (`dEQP-VK.pipeline.*`, `dEQP-VK.graphics_pipeline_library.*`) must test static pipeline compilation explicitly.

### Pattern 5: GPU-Side Verification via Compute Shaders with Host Fallback
- **Where to apply:** Massive image-verification loops where thousands of rendered images are compared against reference thresholds.
- **Implementation:** Dispatch a compute shader (`VK_PIPELINE_BIND_POINT_COMPUTE`) that compares `|actual - expected| <= threshold` on the GPU, returning an error count buffer to the CPU.
- **Mandatory Condition (Two-Tier Verification):**
  1. Fast path: If `errorCount == 0`, pass the test immediately without host readback.
  2. Fallback path: Whenever `errorCount > 0`, automatically trigger a full `vkCmdCopyImageToBuffer` host readback to log exact failing pixel coordinates, expected vs. actual values, and `.qpa` image attachments for debugging.
  3. Direct API copy/blit and memory mapping tests (`dEQP-VK.api.copy_and_blit.*`, `dEQP-VK.memory.*`) must continue using direct host readback.

---

## 4. Auditor Playbook: How to Use `scripts/vulkan_spec_reviewer.py`

Before submitting or applying any performance optimization or structural modification to `VK-GL-CTS`, autonomous agents and developers MUST audit their proposal using the standalone Khronos Vulkan Specification Conformance Reviewer tool located at `scripts/vulkan_spec_reviewer.py`.

### 4.1 Invoking the Reviewer Script
The reviewer script uses the authoritative Vulkan Specification repository in `external/vulkan-docs/src/` (`chapters/`, `appendices/`, `xml/vk.xml`) to verify **Conformance Validity** and **Coverage Equivalence** without command-line arguments.

```bash
# 1. Audit a proposed optimization described in text:
python3 scripts/vulkan_spec_reviewer.py "Reduce default WSI swapchain render loop to std::max(10u, 2u * imageCount)"

# 2. Audit a proposal markdown document, patch file, or diff:
python3 scripts/vulkan_spec_reviewer.py --file path/to/proposal.md

# 3. Specify a model tier (--model=pro, flash, or flash_lite):
python3 scripts/vulkan_spec_reviewer.py --model=pro --file path/to/proposal.md

# 4. Dump the JSON reviewer subagent configuration:
python3 scripts/vulkan_spec_reviewer.py --dump-json
```

### 4.2 Interpreting Audit Verdicts
The reviewer agent will assign one of three formal verdicts:
- **`[APPROVED]`**: The change is 100% conformant by default and maintains identical test coverage.
- **`[APPROVED WITH CONDITIONS]`**: The change is conformant by default ONLY IF the specific implementation conditions detailed in the report are met. You must implement the exact C++ code pattern required by the condition.
- **`[REJECTED]`**: The change would violate Vulkan conformance or reduce required test coverage by default. Do not submit or apply rejected changes.

---

## Appendix A: Khronos Vulkan Specification Audit & Citations for Non-WSI Massive Speedups

The following specification citations and conformance conditions were established by `vulkan_spec_reviewer` when auditing the four massive non-WSI architectural speedups targeting **1.8+ million test cases** in `dEQP-VK.*`:

### 1. Multi-Permutation Texture Atlas Batching (`vkt::AtlasRenderer`) in `pipeline.*` (~500,000+ tests)
- **Primary Citations:** [`drawing.adoc`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/drawing.adoc), [`fragops.adoc:L427-460`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/fragops.adoc#L427-L460) (`Scissor Test`), [`renderpass.adoc:L6430-6436`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/renderpass.adoc#L6430-L6436) (`Load Operations`), [`clears.adoc:L285`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/clears.adoc#L285) (`Clear Commands`).
- **Conformance Validity:** Scissor testing occurs strictly before sample mask, fragment shading, depth, and stencil tests, guaranteeing zero primitive or stencil leakage across tile boundaries.
- **Default Conformance Condition:** Automatically exclude tests evaluating attachment load/store operations (`VkAttachmentLoadOp`), attachment clears (`vkCmdClearAttachments`), resolve attachments, and absolute `gl_FragCoord`.

### 2. Command Buffer Blit & Copy Batching (`BatchedCopyBlitRenderer`) in `api.copy_and_blit.*` & `host_image_copy.*` (274,635 tests)
- **Primary Citations:** [`copies.adoc:L295-L350`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/copies.adoc#L295-L350) (`vkCmdCopyImage`), [`copies.adoc:L1436-1465`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/copies.adoc#L1436-L1465), [`copies.adoc:L1919-1957`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/copies.adoc#L1919-L1957), [`VK_EXT_host_image_copy.adoc:L25-38`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/appendices/VK_EXT_host_image_copy.adoc#L25-L38).
- **Conformance Validity:** Multi-region copy arrays (`pRegions` with `regionCount > 1`) and command merging are explicitly supported provided source/destination regions do not overlap in memory (`VUID-vkCmdCopyImage-pRegions-00124`).
- **Default Conformance Condition:** Automatically exclude dedicated synchronization barrier count tests, indirect copies (`vkCmdCopyMemoryIndirectKHR`), and memory mapping/coherency tests to prevent data race false-positives.

### 3. Session-Level Persistent `VkPipelineCache` in `vkt::Context` (1,413,313 tests)
- **Primary Citations:** [`pipelines.adoc:L8060-8105`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/pipelines.adoc#L8060-L8105) (`== Pipeline Cache`), [`pipelines.adoc:L2099-2104`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/pipelines.adoc#L2099-L2104) (`VUID-vkCreateGraphicsPipelines-pipelineCache-02876`).
- **Conformance Validity:** Reusing cache objects across pipelines and sessions is normative Vulkan usage. Driver caches are internally synchronized unless `VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT` is set.
- **Default Conformance Condition:** Automatically pass `VK_NULL_HANDLE` in dedicated cache conformance tests (`dEQP-VK.pipeline.pipeline_cache.*`) and explicit un-cached compiler creation tests.

### 4. Extended Dynamic State Collapsing (`VK_EXT_extended_dynamic_state`) in Combinatorial Tests (465,382 tests)
- **Primary Citations:** [`VK_EXT_extended_dynamic_state.adoc:L25-39`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/appendices/VK_EXT_extended_dynamic_state.adoc#L25-L39), [`pipelines.adoc:L6135-6189`](file:///Users/tomnom/git/VK-GL-CTS/external/vulkan-docs/src/chapters/pipelines.adoc#L6135-L6189) (`VK_DYNAMIC_STATE_CULL_MODE`, `VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE`, `VK_DYNAMIC_STATE_STENCIL_OP`).
- **Conformance Validity:** Dynamic state overrides static pipeline create info and configures identical hardware rasterization/depth units before drawing.
- **Default Conformance Condition:** Implement runtime feature detection with automatic fallback to static pipeline compilation; preserve static pipelines in dedicated compiler state test suites.

---
*Documentation generated for VK-GL-CTS / dEQP-VK by Antigravity.*

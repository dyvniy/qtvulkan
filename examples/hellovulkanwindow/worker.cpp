/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtVulkan module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "worker.h"
#include <QVulkanFunctions>

void Worker::init()
{
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
        m_cb[i] = VK_NULL_HANDLE;
}

void Worker::cleanup()
{
    QVulkanFunctions *f = m_renderLoop->functions();
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        if (m_cb[i] != VK_NULL_HANDLE) {
            f->vkFreeCommandBuffers(m_renderLoop->device(), m_renderLoop->commandPool(), 1, &m_cb[i]);
            m_cb[i] = VK_NULL_HANDLE;
        }
    }
}

void Worker::queueFrame(int frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem)
{
    qDebug("worker queueFrame %d on thread %p", frame, QThread::currentThread()); // frame = 0 .. frames_in_flight - 1

    // ### just clear to red for now

    QVulkanFunctions *f = m_renderLoop->functions();
    VkImage img = m_renderLoop->currentSwapChainImage();

    // Free the command buffer used in frame no. current - frames_in_flight,
    // we know for sure that that frame has already finished.
    if (m_cb[frame] != VK_NULL_HANDLE)
        f->vkFreeCommandBuffers(m_renderLoop->device(), m_renderLoop->commandPool(), 1, &m_cb[frame]);

    VkCommandBufferAllocateInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_renderLoop->commandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkResult err = f->vkAllocateCommandBuffers(m_renderLoop->device(), &cmdBufInfo, &m_cb[frame]);
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate command buffer: %d", err);

    VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
    err = f->vkBeginCommandBuffer(m_cb[frame], &cmdBufBeginInfo);
    if (err != VK_SUCCESS)
        qFatal("Failed to begin command buffer: %d", err);

    VkClearColorValue clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    VkImageSubresourceRange subResRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    f->vkCmdClearColorImage(m_cb[frame], img, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subResRange);

    err = f->vkEndCommandBuffer(m_cb[frame]);
    if (err != VK_SUCCESS)
        qFatal("Failed to end command buffer: %d", err);

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_cb[frame];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSem;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSem;
    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    submitInfo.pWaitDstStageMask = &psf;
    err = f->vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (err != VK_SUCCESS)
        qFatal("Failed to submit to command queue: %d", err);

#ifndef TEST_ASYNC
    // All command buffer have been submitted with correct wait/signal
    // settings. Notify the renderloop that we are done. We could also have
    // gone async, spawn some threads, and so on. All that matters is to call
    // frameQueued (on any thread) at some point.
    m_renderLoop->frameQueued();
#else
    // Note that one thing we cannot do on this thread is to use timers or
    // other stuff relying on the Qt event loop. This is because while the the
    // thread spins the Qt loop between frames, it will now go to sleep until
    // frameQueued() is called. However, nothing prevents us from spawning as
    // many threads as we want.
    QThreadPool::globalInstance()->start(new AsyncFrameTest(this));
#endif

    // Could schedule updates manually if we did not have UpdateContinuously set:
    // m_renderLoop->update();
}
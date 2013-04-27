/*
 * Copyright (c) 2012-2013 Carsten Munk <carsten.munk@gmail.com>
 *               2012-2013 Simon Busch <morphis@gravedo.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "OffscreenNativeWindow.h"

OffscreenNativeWindow::OffscreenNativeWindow(unsigned int aWidth, unsigned int aHeight, unsigned int aFormat)
	: m_width(aWidth)
	, m_height(aHeight)
	, m_defaultWidth(aWidth)
	, m_defaultHeight(aHeight)
	, m_format(aFormat)
	, m_usage(GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE)
	, m_frontbuffer(NUM_BUFFERS - 1)
	, m_tailbuffer(0)
{
	hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&m_gralloc);
	int err = gralloc_open((hw_module_t*)m_gralloc, &m_alloc);
	TRACE("got alloc %p err:%s\n", m_alloc, strerror(-err));

	for(unsigned int i = 0; i < NUM_BUFFERS; i++)
		m_buffers[i] = 0;
}

OffscreenNativeWindow::~OffscreenNativeWindow()
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
}

int OffscreenNativeWindow::setSwapInterval(int interval)
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return 0;
}

OffscreenNativeWindowBuffer* OffscreenNativeWindow::allocateBuffer()
{
	OffscreenNativeWindowBuffer *buffer = 0;

	buffer = new OffscreenNativeWindowBuffer(width(), height(), m_format, m_usage);

	int usage = buffer->usage;
	usage |= GRALLOC_USAGE_HW_TEXTURE;

	int err = m_alloc->alloc(m_alloc, width(), height(), m_format,
				usage, &buffer->handle, &buffer->stride);

	return buffer;
}

int OffscreenNativeWindow::dequeueBuffer(BaseNativeWindowBuffer **buffer, int *fenceFd)
{
	TRACE("%s\n",__PRETTY_FUNCTION__);

	if(m_buffers[m_tailbuffer] == 0) {
		m_buffers[m_tailbuffer] = allocateBuffer();
		m_buffers[m_tailbuffer]->setIndex(m_tailbuffer);

		TRACE("buffer %i is at %p (native %p) handle=%i stride=%i\n",
				m_tailbuffer, m_buffers[m_tailbuffer], (ANativeWindowBuffer*) m_buffers[m_tailbuffer],
				m_buffers[m_tailbuffer]->handle, m_buffers[m_tailbuffer]->stride);
	}

	OffscreenNativeWindowBuffer *selectedBuffer = m_buffers[m_tailbuffer];
	if (selectedBuffer->width != m_width || selectedBuffer->height != m_height) {
		TRACE("%s buffer and window size doesn't match: resizing buffer ...\n", __PRETTY_FUNCTION__);
		resizeBuffer(m_tailbuffer, selectedBuffer, m_width, m_height);
	}

	*buffer = selectedBuffer;

	waitForBuffer(selectedBuffer);

	m_tailbuffer++;

	if(m_tailbuffer == NUM_BUFFERS)
		m_tailbuffer = 0;

	return NO_ERROR;
}

int OffscreenNativeWindow::lockBuffer(BaseNativeWindowBuffer* buffer)
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return NO_ERROR;
}

int OffscreenNativeWindow::queueBuffer(BaseNativeWindowBuffer* buffer, int fenceFd)
{
	TRACE("%s\n", __PRETTY_FUNCTION__);

	OffscreenNativeWindowBuffer* buf = static_cast<OffscreenNativeWindowBuffer*>(buffer);

	m_frontbuffer++;
	if (m_frontbuffer == NUM_BUFFERS)
		m_frontbuffer = 0;

	postBuffer(buf);

	return NO_ERROR;
}

int OffscreenNativeWindow::cancelBuffer(BaseNativeWindowBuffer* buffer, int fenceFd)
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return 0;
}

unsigned int OffscreenNativeWindow::width() const
{
	TRACE("%s value: %i\n", __PRETTY_FUNCTION__, m_width);
	return m_width;
}

unsigned int OffscreenNativeWindow::height() const
{
	TRACE("%s value: %i\n", __PRETTY_FUNCTION__, m_height);
	return m_height;
}

unsigned int OffscreenNativeWindow::format() const
{
	TRACE("%s value: %i\n", __PRETTY_FUNCTION__, m_format);
	return m_format;
}

unsigned int OffscreenNativeWindow::defaultWidth() const
{
	TRACE("%s value: %i\n", __PRETTY_FUNCTION__, m_defaultWidth);
	return m_defaultWidth;
}

unsigned int OffscreenNativeWindow::defaultHeight() const
{
	TRACE("%s value: %i\n", __PRETTY_FUNCTION__, m_defaultHeight);
	return m_defaultHeight;
}

unsigned int OffscreenNativeWindow::queueLength() const
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return 1;
}

unsigned int OffscreenNativeWindow::type() const
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT;
}

unsigned int OffscreenNativeWindow::transformHint() const
{
	TRACE("%s\n", __PRETTY_FUNCTION__);
	return 0;
}

int OffscreenNativeWindow::setBuffersFormat(int format)
{
	TRACE("%s format %i\n", __PRETTY_FUNCTION__, format);
	m_format = format;
	return NO_ERROR;
}

int OffscreenNativeWindow::setBuffersDimensions(int width, int height)
{
	TRACE("%s size %ix%i\n", __PRETTY_FUNCTION__, width, height);
	return NO_ERROR;
}

int OffscreenNativeWindow::setUsage(int usage)
{
	TRACE("%s usage %i\n", __PRETTY_FUNCTION__, usage);
	m_usage = usage;
	return NO_ERROR;
}

void OffscreenNativeWindow::resize(unsigned int width, unsigned int height)
{
	TRACE("%s width=%i height=%i\n", __PRETTY_FUNCTION__, width, height);

	m_width = width;
	m_defaultWidth = width;
	m_height = height;
	m_defaultHeight = height;

	for (int n = 0; n < NUM_BUFFERS; n++) {
		OffscreenNativeWindowBuffer *buffer = m_buffers[n];

		if (!buffer) {
			TRACE("%s buffer %i isn't used so we ignore it\n", __PRETTY_FUNCTION__, n);
			continue;
		}

		resizeBuffer(n, buffer, width, height);
	}
}

void OffscreenNativeWindow::resizeBuffer(int id, OffscreenNativeWindowBuffer *buffer, unsigned int width,
										 unsigned int height)
{
	if (buffer->handle) {
		TRACE("%s freeing buffer %i ...\n", __PRETTY_FUNCTION__, id);
		m_alloc->free(m_alloc, buffer->handle);
		buffer->handle = 0;
	}

	int usage = buffer->usage;
	usage |= GRALLOC_USAGE_HW_TEXTURE;
	int err = m_alloc->alloc(m_alloc, this->width(), this->height(), m_format,
				usage, &buffer->handle, &buffer->stride);

	buffer->width = width;
	buffer->height = height;
}

int OffscreenNativeWindow::setBufferCount(int count)
{
	return 0;
}

unsigned int OffscreenNativeWindow::bufferCount()
{
	return NUM_BUFFERS;
}

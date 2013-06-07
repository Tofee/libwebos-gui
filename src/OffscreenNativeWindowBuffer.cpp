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
#include <linux/matroxfb.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include <EGL/eglhybris.h>

#include "util/fdpass.h"
#include "OffscreenNativeWindow.h"
#include "BufferAllocator.h"

struct buffer_info_header {
	uint32_t index;
	int32_t width;
	int32_t height;
	int32_t stride;
	int32_t format;
	int32_t usage;
	int32_t num_fds;
	int32_t num_ints;
};

OffscreenNativeWindowBuffer::OffscreenNativeWindowBuffer()
	: m_index(0),
	  m_owner(ownNone),
	  m_busy(false)
{
	ANativeWindowBuffer::width = 0;
	ANativeWindowBuffer::height = 0;
	ANativeWindowBuffer::stride = 0;
	ANativeWindowBuffer::format = 0;
	ANativeWindowBuffer::usage = 0;
	ANativeWindowBuffer::handle = NULL;
}

OffscreenNativeWindowBuffer::OffscreenNativeWindowBuffer(unsigned int width, unsigned int height,
							unsigned int format, unsigned int usage)
	: m_index(0),
	  m_owner(ownData),
	  m_busy(false)
{
	ANativeWindowBuffer::width = width;
	ANativeWindowBuffer::height = height;
	ANativeWindowBuffer::format = format;
	ANativeWindowBuffer::usage = usage;

	BufferAllocator& allocator = BufferAllocator::get();
	allocator.alloc(width, height, format, usage, &handle, &stride);
}

OffscreenNativeWindowBuffer::~OffscreenNativeWindowBuffer()
{
	hybris_unregister_buffer_handle(ANativeWindowBuffer::handle);

	if (handle) {
		if (m_owner == ownHandle) {
			hybris_unregister_buffer_handle(handle);
			native_handle_close(handle);
			native_handle_delete(const_cast<native_handle*>(handle));
		}
		else if (m_owner == ownData) {
			BufferAllocator& allocator = BufferAllocator::get();
			allocator.free(handle);
		}
	}
}

int OffscreenNativeWindowBuffer::writeToFd(int fd)
{
	int ret;
	struct buffer_info_header hdr;

	if (!handle)
		return -EINVAL;

	hdr.index = m_index;
	hdr.width = width;
	hdr.height = height;
	hdr.stride = stride;
	hdr.format = format;
	hdr.usage = usage;
	hdr.num_fds = handle->numFds;
	hdr.num_ints = handle->numInts;

	ret = write(fd, &hdr, sizeof(struct buffer_info_header));
	if (ret < 0)
		return -EIO;

	for (int i = 0; i < handle->numFds; i++) {
		ret = sock_fd_write(fd, (void *)"1", 1, handle->data[i]);
		if (ret < 0)
			return -EIO;
	}

	for (int i = handle->numFds; i < handle->numFds + handle->numInts; i++) {
		ret = write(fd, &handle->data[i], sizeof(handle->data[0]));
		if (ret < 0)
			return -EIO;
	}

	return 0;
}

int OffscreenNativeWindowBuffer::readFromFd(int fd)
{
	struct buffer_info_header hdr;
	int ret;
	char buf[10];

	if (fd < 0)
		return -EINVAL;

	memset(&hdr, 0, sizeof(struct buffer_info_header));

	ret = read(fd, &hdr, sizeof(struct buffer_info_header));
	if (ret < 0 || ret != sizeof(struct buffer_info_header))
		return -EIO;

	printf("Buffer info: width=%i, height=%i, stride=%i, format=%i usage=%i numFds=%i, numInts=%i\n",
		   hdr.width, hdr.height, hdr.stride, hdr.format, hdr.usage, hdr.num_fds, hdr.num_ints);

	m_index = hdr.index;
	width = hdr.width;
	height = hdr.height;
	stride = hdr.stride;
	format = hdr.format;
	usage = hdr.usage;

	if (handle) {
		native_handle_close(handle);
		native_handle_delete(const_cast<native_handle_t*>(handle));
	}

	handle = native_handle_create(hdr.num_fds, hdr.num_ints);
	for (int i = 0; i < hdr.num_fds; i++) {
		ret = sock_fd_read(fd, buf, sizeof(buf), (int *) &handle->data[i]);
		if (ret < 0)
			return -EIO;
	}

	for (int i = hdr.num_fds; i < hdr.num_fds + hdr.num_ints; i++) {
		ret = read(fd, (void *) &handle->data[i], sizeof(handle->data[i]));
		if (ret < 0)
			return -EIO;
	}

	hybris_register_buffer_handle(this->handle);

	m_owner = ownHandle;

	printf("Successfully received buffer from remote\n");

	return 0;
}

buffer_handle_t OffscreenNativeWindowBuffer::getHandle()
{
	return handle;
}

// vim:ts=4:sw=4:noexpandtab

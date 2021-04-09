/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
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
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <glib.h>
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_recovery.h"

ctrlm_recovery_status_t ctrlm_recovery_shared_memory_open();
void                    ctrlm_recovery_shared_memory_close(gboolean unlink);

static ctrlm_recovery_shared_memory_t *g_ctrlm_recovery_shared_memory;
static gint                            g_ctrlm_recovery_shared_memory_fd;

gboolean ctrlm_recovery_init() {
   LOG_INFO("%s\n", __FUNCTION__);
   g_ctrlm_recovery_shared_memory    = NULL;
   g_ctrlm_recovery_shared_memory_fd = -1;

   if(CTRLM_RECOVERY_STATUS_SUCCESS != ctrlm_recovery_shared_memory_open()) {
      LOG_ERROR("%s: Failed to initialize shared memory\n", __FUNCTION__);
      return FALSE;
   }
   return TRUE;
}

void ctrlm_recovery_property_set(ctrlm_recovery_property_t property, void *value) {

   if(NULL == g_ctrlm_recovery_shared_memory) {
      LOG_ERROR("%s: Recovery was not initialized properly.. Cannot set property %d\n", __FUNCTION__, property);
      return;
   }

   switch(property) {
      case CTRLM_RECOVERY_CRASH_COUNT: 
      {
         guint  *crash_count = (guint *)value;
         g_ctrlm_recovery_shared_memory->crash_count = *crash_count;
         break;

      }
      case CTRLM_RECOVERY_INVALID_HAL_NVM:
      {
         guint  *flag = (guint *)value;
         g_ctrlm_recovery_shared_memory->invalid_hal_nvm = *flag;
         break;
      }
      case CTRLM_RECOVERY_INVALID_CTRLM_DB:
      {
         guint  *flag = (guint *)value;
         g_ctrlm_recovery_shared_memory->invalid_ctrlm_db = *flag;
         break;
      }
      default:
      {
         break;
      }
   }
}

void ctrlm_recovery_property_get(ctrlm_recovery_property_t property, void *value) {
   if(NULL == g_ctrlm_recovery_shared_memory) {
      LOG_ERROR("%s: Recovery was not initialized properly.. Cannot set property %d\n", __FUNCTION__, property);
      return;
   }

   switch(property) {
      case CTRLM_RECOVERY_CRASH_COUNT: 
      {
         guint  *crash_count = (guint *)value;
         *crash_count = g_ctrlm_recovery_shared_memory->crash_count;
         break;
      }
      case CTRLM_RECOVERY_INVALID_HAL_NVM:
      {
         guint *flag = (guint *)value;
         *flag = g_ctrlm_recovery_shared_memory->invalid_hal_nvm;
         break;
      }
      case CTRLM_RECOVERY_INVALID_CTRLM_DB:
      {
         guint *flag = (guint *)value;
         *flag = g_ctrlm_recovery_shared_memory->invalid_ctrlm_db;
         break;
      }
      default:
      {
         break;
      }
   }
}

void ctrlm_recovery_factory_reset() {
   remove(CTRLM_NVM_BACKUP);
   remove(HAL_NVM_BACKUP);
}

void ctrlm_recovery_terminate(gboolean unlink) {
   ctrlm_recovery_shared_memory_close(unlink);
}

ctrlm_recovery_status_t ctrlm_recovery_shared_memory_open() {
   gboolean created = FALSE;
            errno = 0;
 
   if(g_ctrlm_recovery_shared_memory_fd < 0) {
      errno = 0;
      g_ctrlm_recovery_shared_memory_fd = shm_open(CTRLM_RECOVERY_SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
 
      if(g_ctrlm_recovery_shared_memory_fd < 0) {
         int errsv = errno;
         LOG_ERROR("%s: Unable to open shared memory <%s>\n", __FUNCTION__, strerror(errsv));
         return CTRLM_RECOVERY_STATUS_FAILURE;
      }
   }
 
   struct stat buf;
   errno = 0;
   if(fstat(g_ctrlm_recovery_shared_memory_fd, &buf)) {
      int errsv = errno;
      LOG_ERROR("%s: Unable to stat shared memory <%s>\n", __FUNCTION__, strerror(errsv));
      ctrlm_recovery_shared_memory_close(TRUE);
      return CTRLM_RECOVERY_STATUS_FAILURE;
   }
 
   if(0 == buf.st_size) {
      LOG_INFO("%s: Created shared memory region\n", __FUNCTION__);
      errno = 0;
      if(ftruncate(g_ctrlm_recovery_shared_memory_fd, sizeof(ctrlm_recovery_shared_memory_t)) < 0) {
         int errsv = errno;
         LOG_ERROR("%s: Unable to set shared memory size %d <%s>\n", __FUNCTION__, g_ctrlm_recovery_shared_memory_fd, strerror(errsv));
         ctrlm_recovery_shared_memory_close(TRUE);
         return CTRLM_RECOVERY_STATUS_FAILURE;
      }
      created = TRUE;
   }

   if(NULL == g_ctrlm_recovery_shared_memory) {
      errno = 0;
      g_ctrlm_recovery_shared_memory = (ctrlm_recovery_shared_memory_t *)mmap(NULL, sizeof(ctrlm_recovery_shared_memory_t), (PROT_READ | PROT_WRITE), MAP_SHARED, g_ctrlm_recovery_shared_memory_fd, 0);

      if(NULL == g_ctrlm_recovery_shared_memory) {
         int errsv = errno;
         LOG_ERROR("%s: unable to map shared memory %d <%s>\n", __FUNCTION__, g_ctrlm_recovery_shared_memory_fd, strerror(errsv));
         ctrlm_recovery_shared_memory_close(TRUE);
         return CTRLM_RECOVERY_STATUS_FAILURE;
      }

      if(created) {
         memset(g_ctrlm_recovery_shared_memory, 0, sizeof(ctrlm_recovery_shared_memory_t));
      }
   }

   LOG_INFO("%s: Shared memory open successful\n", __FUNCTION__);

   return CTRLM_RECOVERY_STATUS_SUCCESS;
}

void ctrlm_recovery_shared_memory_close(gboolean unlink) {

   if(NULL != g_ctrlm_recovery_shared_memory) {
      errno = 0;
      if(munmap(g_ctrlm_recovery_shared_memory, sizeof(ctrlm_recovery_shared_memory_t)) < 0) {
         int errsv = errno;
         LOG_WARN("%s: unable to unmap shared memory %d <%s>\n", __FUNCTION__, g_ctrlm_recovery_shared_memory_fd, strerror(errsv));
      }
      g_ctrlm_recovery_shared_memory = NULL;
   }

   if(g_ctrlm_recovery_shared_memory_fd > 0) {
      errno = 0;
      if(close(g_ctrlm_recovery_shared_memory_fd) < 0) {
         int errsv = errno;
         LOG_WARN("%s: unable to close shared memory %d <%s>\n", __FUNCTION__, g_ctrlm_recovery_shared_memory_fd, strerror(errsv));
      }
      g_ctrlm_recovery_shared_memory_fd = -1;
   }

   if(unlink) {
      errno = 0;
      if(shm_unlink(CTRLM_RECOVERY_SHARED_MEMORY_NAME) < 0) {
         int errsv = errno;
         LOG_ERROR("%s: unable to unlink shared memory <%s>\n", __FUNCTION__, strerror(errsv));
      }
   }

}

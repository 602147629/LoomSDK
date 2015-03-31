/*
 * ===========================================================================
 * Loom SDK
 * Copyright 2011, 2012, 2013
 * The Game Engine Company, LLC
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
 * ===========================================================================
 */

#include "loom/common/assets/assets.h"
#include "loom/common/assets/assetsImage.h"

#include "loom/common/core/assert.h"
#include "loom/common/core/allocator.h"
#include "loom/common/core/log.h"
#include "loom/common/utils/utTypes.h"

#include "loom/graphics/gfxGraphics.h"
#include "loom/graphics/gfxTexture.h"

#include "loom/common/platform/platformTime.h"

lmDefineLogGroup(gGFXTextureLogGroup, "GFXTexture", 1, LoomLogInfo);
loom_allocator_t *gGFXTextureAllocator = NULL;

namespace GFX
{

TextureInfo Texture::sTextureInfos[MAXTEXTURES];
utHashTable<utFastStringHash, TextureID> Texture::sTexturePathLookup;
bool Texture::sTextureAssetNofificationsEnabled = true;
bool Texture::supportsFullNPOT;

//queue of textures to load in the async loading thread
utList<AsyncLoadNote> Texture::sAsyncLoadQueue;

//queue of loaded texture data to be created back in the main thread
utList<AsyncLoadNote> Texture::sAsyncCreateQueue;

//current frame delay counter used to space out texture creation between frames
int Texture::sAsyncTextureCreateDelay = 0;

//flag indicating if the async loading thread is currently running
bool Texture::sAsyncThreadRunning = false;

//mutex used for locking sAsyncLoadQueue and sAsyncCreateQueue between threads
MutexHandle Texture::sAsyncQueueMutex = NULL;

//mutex used for locking sTextureInfos and sTexturePathLookup between threads
MutexHandle Texture::sTexInfoLock = NULL;


void Texture::initialize()
{
    for (int i = 0; i < MAXTEXTURES; i++)
    {
        sTextureInfos[i].id         = i;
        sTextureInfos[i].reload     = false;
        sTextureInfos[i].asyncDispose = false;
        sTextureInfos[i].handle     = -1;
    }
    Texture::sTexInfoLock = loom_mutex_create();
    Texture::sAsyncQueueMutex = loom_mutex_create();

#if LOOM_RENDERER_OPENGLES2
    Texture::supportsFullNPOT = Graphics::queryExtension("GL_ARB_texture_non_power_of_two") || Graphics::queryExtension("GL_OES_texture_npot");
#else
    Texture::supportsFullNPOT = true;
#endif

}

void Texture::tick()
{
    int startTime;

    //process any textures queued up for creation inside of the async load thread
    //only process a new texture after so much time has elapsed so that we don't bog the main thread down
    if(--Texture::sAsyncTextureCreateDelay <= 0)
    {
        Texture::sAsyncTextureCreateDelay = 20;
        loom_mutex_lock(Texture::sAsyncQueueMutex);
        if(!Texture::sAsyncCreateQueue.empty())
        {
            //get the note containing the information for this texture
            AsyncLoadNote threadNote = Texture::sAsyncCreateQueue.front();
            Texture::sAsyncCreateQueue.pop_front();
            loom_mutex_unlock(Texture::sAsyncQueueMutex);

            //handleAssetNotification does the actual creation of the texture data immediately below when '1' is specified
            startTime = platform_getMilliseconds();
            if(!threadNote.path.empty())
            {
                //Texture is an Asset, so Create via handleAssetNotification
                loom_asset_subscribe(threadNote.path.c_str(), Texture::handleAssetNotification, (void *)threadNote.id, 1);
                lmLog(gGFXTextureLogGroup, "Async loaded texture '%s' took %i ms to create", threadNote.path.c_str(), platform_getMilliseconds() - startTime);
            }
            else
            {
                //Texture is just a byte stream, so load the deserialized image data now
                loadImageAsset(threadNote.imageAsset, threadNote.id);
                threadNote.iaCleanup(threadNote.imageAsset);
                lmLog(gGFXTextureLogGroup, "Async loaded byte texture took %i ms to create", platform_getMilliseconds() - startTime);
            }

            //were we disposed while we were busy loading?
            loom_mutex_lock(Texture::sTexInfoLock);
            int disposeID = (!threadNote.tinfo->asyncDispose) ? -1 : threadNote.id;
            loom_mutex_unlock(Texture::sTexInfoLock);
            if(disposeID != -1)
            {
                //dispose!
                Texture::dispose(disposeID);
            }
            else
            {
                //Fire the async load complete delegate... not if we were destroyed while loading though
                threadNote.tinfo->asyncLoadCompleteDelegate.invoke();
            }    
        }
        else
        {
            loom_mutex_unlock(Texture::sAsyncQueueMutex);
        }
    }
}


static void rgbaToBgra(uint8_t *_data, uint32_t _width, uint32_t _height)
{
    uint32_t dstpitch = _width * 4;

    for (uint32_t yy = 0; yy < _height; ++yy)
    {
        uint8_t *dst = &_data[yy * dstpitch];

        for (uint32_t xx = 0; xx < _width; ++xx)
        {
            uint8_t tmp = dst[0];
            dst[0] = dst[2];
            dst[2] = tmp;
            dst   += 4;
        }
    }
}


// Courtesy of Torque via MIT license.
void bitmapExtrudeRGBA_c(const void *srcMip, void *mip, int srcHeight, int srcWidth)
{
    const unsigned char *src = (const unsigned char *)srcMip;
    unsigned char       *dst = (unsigned char *)mip;
    int                 stride = srcHeight != 1 ? (srcWidth)* 4 : 0;

    int width = srcWidth >> 1;
    int height = srcHeight >> 1;

    if (width == 0)
    {
        width = 1;
    }
    if (height == 0)
    {
        height = 1;
    }

    if (srcWidth != 1)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                *dst++ = (int(*src) + int(src[4]) + int(src[stride]) + int(src[stride + 4]) + 2) >> 2;
                src++;
                *dst++ = (int(*src) + int(src[4]) + int(src[stride]) + int(src[stride + 4]) + 2) >> 2;
                src++;
                *dst++ = (int(*src) + int(src[4]) + int(src[stride]) + int(src[stride + 4]) + 2) >> 2;
                src++;
                *dst++ = (int(*src) + int(src[4]) + int(src[stride]) + int(src[stride + 4]) + 2) >> 2;
                src += 5;
            }
            src += stride;    // skip
        }
    }
    else
    {
        for (int y = 0; y < height; y++)
        {
            *dst++ = (int(*src) + int(src[stride]) + 1) >> 1;
            src++;
            *dst++ = (int(*src) + int(src[stride]) + 1) >> 1;
            src++;
            *dst++ = (int(*src) + int(src[stride]) + 1) >> 1;
            src++;
            *dst++ = (int(*src) + int(src[stride]) + 1) >> 1;
            src += 5;

            src += stride;    // skip
        }
    }
}


TextureInfo *Texture::load(uint8_t *data, uint16_t width, uint16_t height, TextureID id)
{
    if (id == -1)
    {
        id = getAvailableTextureID();

        if (id == TEXTUREINVALID)
        {
            return NULL;
        }
    }

    if ((id < 0) || (id >= MAXTEXTURES))
    {
        return NULL;
    }

    loom_mutex_lock(Texture::sTexInfoLock);

    TextureInfo &tinfo = sTextureInfos[id];

    bool newTexture = !tinfo.reload || (tinfo.width != width) || (tinfo.height != height);

    if (newTexture)
    {
        lmLog(gGFXTextureLogGroup, "Create texture for %s", tinfo.texturePath.c_str());
    }
    else
    {
        lmLog(gGFXTextureLogGroup, "Updating texture %s", tinfo.texturePath.c_str());
    }


    if (tinfo.reload)
    {
        Graphics::context()->glDeleteTextures(1, &tinfo.handle);
    }

    if (newTexture) Graphics::context()->glGenTextures(1, &tinfo.handle);

    lmLog(gGFXTextureLogGroup, "Create > %u", tinfo.handle);

    //lmLog(gGFXTextureLogGroup, "OpenGL error %d", Graphics::context()->glGetError());

    Graphics::context()->glBindTexture(GL_TEXTURE_2D, tinfo.handle);

    Graphics::context()->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /*
    Graphics::context()->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    Graphics::context()->glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    Graphics::context()->glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    */
    Graphics::context()->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    /*
    Graphics::context()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Graphics::context()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Graphics::context()->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Graphics::context()->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    */
    //lmLogInfo(gGFXTextureLogGroup, "OpenGL error %d", Graphics::context()->glGetError());

    tinfo.width  = width;
    tinfo.height = height;

    if (supportsFullNPOT || tinfo.isPowerOfTwo())
    {
        tinfo.clampOnly = false;
        tinfo.mipmaps = true;
        uint8_t *mipData = data;
        int mipWidth = width;
        int mipHeight = height;
        int mipLevel = 1;
        //uint64_t time = GetTimeMs64();
        while (mipWidth > 1 || mipHeight > 1)
        {
            // Allocate new bits.
            int prevWidth = mipWidth, prevHeight = mipHeight;
            mipWidth >>= 1; mipWidth = mipWidth < 1 ? 1 : mipWidth;
            mipHeight >>= 1; mipHeight = mipHeight < 1 ? 1 : mipHeight;

            uint8_t *prevData = mipData;
            mipData = static_cast<uint8_t*>(lmAlloc(NULL, mipWidth * mipHeight * 4));

            bitmapExtrudeRGBA_c(prevData, mipData, prevHeight, prevWidth);
            if (prevData != data) lmFree(NULL, prevData);

            Graphics::context()->glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_RGBA, mipWidth, mipHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, mipData);

            mipLevel++;
        }
        if (mipData != data) lmFree(NULL, mipData);
    } else {
        tinfo.clampOnly = true;
        tinfo.mipmaps = false;
        lmLogWarn(gGFXTextureLogGroup, "Non-power-of-two textures not fully supported by device, consider using a power-of-two texture size")
    }

    //lmLogInfo(gGFXTextureLogGroup, "Generated mipmaps in %d ms", Graphics::context()->glGetError());
    //*/

    if (tinfo.reload)
    {
        // Fire the delegate.
        tinfo.updateDelegate.pushArgument(width);
        tinfo.updateDelegate.pushArgument(height);
        tinfo.updateDelegate.invoke();
    }

    // mark that next time we will be reloading
    tinfo.reload = true;

    loom_mutex_unlock(Texture::sTexInfoLock);

    return &tinfo;
}

TextureInfo *Texture::initFromAssetManager(const char *path)
{
    if (!path || !path[0])
    {
        return NULL;
    }

    //check if this texture already has a TextureInfo reserved for it
    TextureID *texID;
    TextureInfo *tinfo = getTextureInfoFromPath(path, &texID);
    if(texID)
        return tinfo;

    // Force it to load.
    if(loom_asset_lock(path, LATImage, 1) == NULL)
    {
        lmLogWarn(gGFXTextureLogGroup, "Unable to lock the asset for texture %s", path);
        return NULL;
    }
    loom_asset_unlock(path);

    // Get a new texture info
    tinfo = getAvailableTextureInfo(path);
    if(tinfo != NULL)
    {
        // allocate the texture handle/id
        lmLog(gGFXTextureLogGroup, "loading %s", path);

        // Now subscribe and let us load for reals.
        loom_asset_subscribe(path, Texture::handleAssetNotification, (void *)tinfo->id, 1);        
    }
    else
    {
        lmLog(gGFXTextureLogGroup, "No available texture id for %s", path);
    }
    return tinfo;
}


TextureInfo *Texture::initFromBytes(utByteArray *bytes, const char *name)
{
    TextureInfo *tinfo = NULL;
    name = (name && !name[0]) ? NULL : name;
    if(name)
    {
        //check if this texture already has a TextureInfo reserved for it
        TextureID *texID;
        TextureInfo *tinfo = getTextureInfoFromPath(name, &texID);
        if(texID)
        {
            return tinfo;
        }
    }

    // Get a new texture info
    tinfo = getAvailableTextureInfo(name);
    if(tinfo == NULL)
    {
        lmLog(gGFXTextureLogGroup, "No available texture id for image bytes");
        return NULL;
    }

    //load the byte stream
    LoomAssetCleanupCallback dtor = NULL;
    loom_asset_image_t *lat = static_cast<loom_asset_image_t*>(loom_asset_imageDeserializer(bytes->getDataPtr(), bytes->getSize(), &dtor));

    if (lat == NULL) {
        lmLog(gGFXTextureLogGroup, "Unable to load image bytes");
        return NULL;
    }

    // Great, stuff real bits!
    lmLog(gGFXTextureLogGroup, "loaded image bytes - %i x %i at id %i", lat->width, lat->height, tinfo->id);

    loadImageAsset(lat, tinfo->id);

    dtor(lat);

    return tinfo;
}


int __stdcall Texture::loadTextureAsync_body(void *param)
{
    bool addToQueue;
    const char *path = NULL;

    //remain in a loop here so long as we have notes to process
    while(true)
    {
        addToQueue = true;

        //get the front of the async texture queue to process
        loom_mutex_lock(Texture::sAsyncQueueMutex);
        AsyncLoadNote threadNote = Texture::sAsyncLoadQueue.front();
        Texture::sAsyncLoadQueue.pop_front();
        loom_mutex_unlock(Texture::sAsyncQueueMutex);
        path = (!threadNote.path.empty()) ? threadNote.path.c_str() : NULL;

        //make sure we weren't disposed in the meantime... if so, just skip!
        loom_mutex_lock(Texture::sTexInfoLock);
        bool alreadyDisposed = threadNote.tinfo->asyncDispose;
        if(alreadyDisposed)
        {
            //invalidate the TextureInfo and make it available for use again
            if (!threadNote.tinfo->texturePath.empty()) 
            {
                sTexturePathLookup.erase(threadNote.tinfo->texturePath);
            }
            threadNote.tinfo->reset();
            loom_mutex_unlock(Texture::sTexInfoLock);

            //prep for below
            loom_mutex_lock(Texture::sAsyncQueueMutex);
        }
        else
        {
            loom_mutex_unlock(Texture::sTexInfoLock);
            
            //handle Asset vs ByteArray texture load
            if(path)
            {
                // Load async since we're in a background thread.
                lmLog(gGFXTextureLogGroup, "loading %s async...", path);
                loom_asset_preload(path);
                loom_thread_yield();
                loom_asset_image *lai = NULL;
                while((lai = (loom_asset_image *)loom_asset_lock(path, LATImage, 0)) == NULL)
                {
                    loom_thread_yield();
                }
                loom_asset_unlock(path);
            }
            else
            {
                //deserialize the image data from bytes
                threadNote.imageAsset = static_cast<loom_asset_image_t*>(loom_asset_imageDeserializer(threadNote.bytes->getDataPtr(), 
                                                                                                        threadNote.bytes->getSize(), 
                                                                                                        &threadNote.iaCleanup));
                if (threadNote.imageAsset == NULL) 
                {
                    lmLog(gGFXTextureLogGroup, "ERROR: Unable to deserialize image bytes!");
                    addToQueue = false;
                }
            }

            //add to the CreateQueue that happens in the main thread because bgfx cannot create textures from side threads
            loom_mutex_lock(Texture::sAsyncQueueMutex);
            if(addToQueue)
            {
                lmLog(gGFXTextureLogGroup, "Adding async loaded texture to CreateQueue: %s", ((path) ? path : "Byte Texture"));

                //add to the front of the queue if high priority, otherwise, FIFO
                if(threadNote.priority)
                {
                    sAsyncCreateQueue.push_front(threadNote);
                }
                else
                {
                    sAsyncCreateQueue.push_back(threadNote);
                }
            }
        }

        //do we have any more items to process?
        if(Texture::sAsyncLoadQueue.empty())
        {
            //flag that the async texture thread is no longer running
            Texture::sAsyncThreadRunning = false;
            loom_mutex_unlock(Texture::sAsyncQueueMutex);
            break;
        }
        loom_mutex_unlock(Texture::sAsyncQueueMutex);

        //yield to the main thread, baby!
        loom_thread_yield();
    }

    return 0;
}


TextureInfo * Texture::initFromAssetManagerAsync(const char *path, bool highPriority)
{
    if (!path || !path[0])
    {
        lmLogError(gGFXTextureLogGroup, "Empty Path specified for initFromAssetManagerAsync()");
        return NULL;
    }

    //check if this texture already has a TextureInfo reserved for it
    //NOTE: shouldn't really happen... there is a check for this in LS that returns early there!    
    TextureID   *texID;
    TextureInfo *tinfo = getTextureInfoFromPath(path, &texID);
    if(texID)
    {
        return tinfo;
    }

    // Get a new texture info
    tinfo = getAvailableTextureInfo(path);
    if(tinfo != NULL)
    {
         //build up temp struct to pass over to the aysnc load thread
        AsyncLoadNote threadNote;
        memset(&threadNote, 0, sizeof(AsyncLoadNote));
        threadNote.id = tinfo->id;
        threadNote.path = path;
        threadNote.tinfo = tinfo;
        threadNote.priority = highPriority;

        //add this texture to async queue
        loom_mutex_lock(Texture::sAsyncQueueMutex);

        //add to the front of the queue if high priority, otherwise, FIFO
        if(highPriority)
        {
            sAsyncLoadQueue.push_front(threadNote);
        }
        else
        {
            sAsyncLoadQueue.push_back(threadNote);
        }
        if(!Texture::sAsyncThreadRunning)
        {
            //only kick the async thread if it isn't already running
            Texture::sAsyncThreadRunning = true;
            loom_thread_start(Texture::loadTextureAsync_body, NULL);
        }
        loom_mutex_unlock(Texture::sAsyncQueueMutex); 
    }
    else
    {
        lmLog(gGFXTextureLogGroup, "No available texture id for %s", path);
    }
    return tinfo;
}


TextureInfo *Texture::initFromBytesAsync(utByteArray *bytes, const char *name, bool highPriority)
{
    TextureInfo *tinfo = NULL;
    name = (name && !name[0]) ? NULL : name;
    if(name)
    {
        //check if this texture already has a TextureInfo reserved for it
        //NOTE: shouldn't really happen... there is a check for this in LS that returns early there!    
        loom_mutex_lock(Texture::sTexInfoLock);
        TextureID   *pid   = sTexturePathLookup.get(name);
        TextureInfo *tinfo = (pid && ((*pid >= 0) && (*pid < MAXTEXTURES))) ? &sTextureInfos[*pid] : NULL;
        if(pid && (!tinfo || (tinfo->handle == -1)))
        {
            lmLogError(gGFXTextureLogGroup, "Invalid Texture ID or Handle returned in initFromBytesAsync() for texture: %s", name);
            tinfo = NULL;
        }
        if(pid)
        {
            if(tinfo && tinfo->asyncDispose)
            {
                lmLog(gGFXTextureLogGroup, "Returning a TextureInfo that was flagged for disposal during initFromBytesAsync() for texture: %s", name);
                tinfo->asyncDispose = false;
            }
            loom_mutex_unlock(Texture::sTexInfoLock);
            return tinfo;
        }
        loom_mutex_unlock(Texture::sTexInfoLock);
    }

    // Get a new texture info
    tinfo = getAvailableTextureInfo(name);
    if(tinfo != NULL)
    {
         //build up temp struct to pass over to the aysnc load thread
        AsyncLoadNote threadNote;
        memset(&threadNote, 0, sizeof(AsyncLoadNote));
        threadNote.id = tinfo->id;
        threadNote.path = "";
        threadNote.tinfo = tinfo;
        threadNote.bytes = bytes;
        threadNote.priority = highPriority;

        //add this texture to async queue
        loom_mutex_lock(Texture::sAsyncQueueMutex);

        //add to the front of the queue if high priority, otherwise, FIFO
        if(highPriority)
        {
            sAsyncLoadQueue.push_front(threadNote);
        }
        else
        {
            sAsyncLoadQueue.push_back(threadNote);
        }
        if(!Texture::sAsyncThreadRunning)
        {
            //only kick the async thread if it isn't already running
            Texture::sAsyncThreadRunning = true;
            loom_thread_start(Texture::loadTextureAsync_body, NULL);
        }
        loom_mutex_unlock(Texture::sAsyncQueueMutex); 
    }
    else
    {
        lmLog(gGFXTextureLogGroup, "No available texture id for image bytes");
    }
    return tinfo;
}


void Texture::loadCheckerBoard(TextureID id)
{
    const int checkerboardSize = 128, checkSize = 8;        

    int *checkerboard = (int*)lmAlloc(gGFXTextureAllocator, checkerboardSize*checkerboardSize*4);

    for (int i = 0; i < checkerboardSize; i++)
    {
        for (int j = 0; j < checkerboardSize; j++)
        {
            checkerboard[(i * checkerboardSize) + j] = (((i / checkSize) ^ (j / checkSize)) & 1) == 0 ? 0xFF00FFFF : 0x00FF0077;
        }
    }

    load((uint8_t *)checkerboard, checkerboardSize, checkerboardSize, id);

    lmFree(gGFXTextureAllocator, checkerboard);
}

void Texture::handleAssetNotification(void *payload, const char *name)
{
    TextureID id = (TextureID)payload;

    if (!sTextureAssetNofificationsEnabled)
    {
        lmLogError(gGFXTextureLogGroup, "Attempting to load texture while notifications are disabled '%s', using debug checkerboard.", name);        
        loadCheckerBoard(id);
        return;
    }

    // Get the image via the asset manager.    
    loom_asset_image_t *lat = (loom_asset_image_t *)loom_asset_lock(name, LATImage, 0);

    // If we couldn't load it, and we have never loaded it, generate a checkerboard placeholder texture.
    if (!lat)
    {
        loom_mutex_lock(Texture::sTexInfoLock);
        bool reload = sTextureInfos[id].reload;
        loom_mutex_unlock(Texture::sTexInfoLock);
        if(reload)
            return;

        lmLogError(gGFXTextureLogGroup, "Missing image asset '%s', using %dx%d px debug checkerboard.", name, 128, 128);

        loadCheckerBoard(id);

        return;
    }

    // Great, stuff real bits!
    lmLog(gGFXTextureLogGroup, "loaded %s - %i x %i at id %i", name, lat->width, lat->height, id);

    loadImageAsset(lat, id);

    // Release lock on the asset.
    loom_asset_unlock(name);
    
    // Once we load it we don't need it any more.
    loom_asset_flush(name);
}

void Texture::loadImageAsset(loom_asset_image_t *lat, TextureID id)
{
    // See if it's over 2048 - if so, downsize to fit.
    const int          maxSize     = 2048;
    void               *localBits  = lat->bits;
    void               *localMem   = NULL;
    int                localWidth  = lat->width;
    int                localHeight = lat->height;
    while (localWidth > maxSize || localHeight > maxSize)
    {
        // Allocate new bits.
        int oldWidth = localWidth, oldHeight = localHeight;
        localWidth = localWidth >> 1;
        localHeight = localHeight >> 1;
        void *oldBits = localBits;

        // This will be freed automatically. This will be inefficient for huge bitmaps but it's
        // only around for one frame.
        localBits = lmAlloc(NULL, localWidth * localHeight * 4);

        lmLog(gGFXTextureLogGroup, "   - Too big! Downsampling to %dx%d", localWidth, localHeight);

        bitmapExtrudeRGBA_c(oldBits, localBits, oldHeight, oldWidth);
    }

    // Perform the actual load.
    load((uint8_t *)localBits, (uint16_t)localWidth, (uint16_t)localHeight, id);

// TODO: Memory leak due to resize loop.
//    lmFree(NULL, localBits);
}

void Texture::reset()
{
    for (int i = 0; i < MAXTEXTURES; i++)
    {
        loom_mutex_lock(Texture::sTexInfoLock);
        TextureInfo *tinfo = &sTextureInfos[i];

        // Ignore invalid entries.
        if (sTextureInfos[i].handle == -1)
        {
            const char *path = tinfo->texturePath.c_str();
            lmLog(gGFXTextureLogGroup, "Reloading texture for path %s", path);

            Graphics::context()->glDeleteTextures(1, &tinfo->handle);
            tinfo->handle     = -1;
            tinfo->reload     = false;

            loom_mutex_unlock(Texture::sTexInfoLock);

            //force it to be loaded from disk
            loom_asset_lock(path, LATImage, 1);
            loom_asset_unlock(path);

            //do actual texture creation/update
            handleAssetNotification((void *)tinfo->id, path);
        }
        else
        {
            loom_mutex_unlock(Texture::sTexInfoLock);            
        }
    }
}


void Texture::dispose(TextureID id)
{
    if ((id < 0) || (id >= MAXTEXTURES))
    {
        return;
    }

    loom_mutex_lock(Texture::sTexInfoLock);
    TextureInfo *tinfo = &sTextureInfos[id];

    // If the texture isn't valid ignore it.
    if (tinfo->handle == -1)
    {
        //if texture is still loading or is inside of the loading queue, we can't dispose of it now, 
        //but need to flag it for disposal in the thread
        if(tinfo->handle == MARKEDTEXTURE)
        {
            sTextureInfos[id].asyncDispose = true;
            loom_mutex_unlock(Texture::sTexInfoLock);
            return;
        }

        //TODO: LOOM-1653, we really shouldn't be holding a copy of the texture data in the
        // asset system until we dispose
        if (!tinfo->texturePath.empty()) {
            loom_asset_unsubscribe(tinfo->texturePath.c_str(), handleAssetNotification, (void *)id);
            loom_asset_flush(tinfo->texturePath.c_str());

            // Reset the hash, too
            sTexturePathLookup.erase(tinfo->texturePath);
        }

        // And erase backing state.
        Graphics::context()->glDeleteTextures(1, &tinfo->handle);
        tinfo->reset();
    }
    loom_mutex_unlock(Texture::sTexInfoLock);
}
}

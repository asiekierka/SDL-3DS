/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Dummy SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include <3ds.h>
#include <citro3d.h>

#include "SDL_n3dsvideo.h"
#include "SDL_n3dsevents_c.h"
#include "SDL_n3dsmouse_c.h"

#define N3DSVID_DRIVER_NAME "Nintendo 3DS video driver"

/* Low level N3ds */

// This header is generated by the build process
#include "vshader_shbin.h"

#define TEX_MIN_SIZE 32
#define CLEAR_COLOR0 0x000000FF
#define CLEAR_COLOR1 0x000000
#define CLEAR_COLOR2 0x000000
#define CLEAR_COLOR3 0x000001

// Used to transfer the final rendered display to the framebuffer
#define DISPLAY_TRANSFER_FLAGS0 \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define DISPLAY_TRANSFER_FLAGS1 \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define DISPLAY_TRANSFER_FLAGS2 \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define DISPLAY_TRANSFER_FLAGS3 \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB5A1) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// Used to convert textures to 3DS tiled format
// Note: vertical flip flag set so 0,0 is top left of texture
#define TEXTURE_TRANSFER_FLAGS0 \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define TEXTURE_TRANSFER_FLAGS1 \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define TEXTURE_TRANSFER_FLAGS2 \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
#define TEXTURE_TRANSFER_FLAGS3 \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB5A1) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB5A1) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;
static C3D_Mtx projection2;
static bool drawing = false;

C3D_RenderTarget *VideoSurface1;
C3D_RenderTarget *VideoSurface2;
static C3D_Tex spritesheet_tex;

struct SDL_PrivateVideoData * pvd = NULL;

static int textureTranferFlags[4] = { TEXTURE_TRANSFER_FLAGS0, TEXTURE_TRANSFER_FLAGS1, TEXTURE_TRANSFER_FLAGS2, TEXTURE_TRANSFER_FLAGS3};
static int displayTranferFlags[4] = { DISPLAY_TRANSFER_FLAGS0, DISPLAY_TRANSFER_FLAGS1, DISPLAY_TRANSFER_FLAGS2, DISPLAY_TRANSFER_FLAGS3};
static unsigned int clearcolors[4] = { CLEAR_COLOR0, CLEAR_COLOR1, CLEAR_COLOR2, CLEAR_COLOR3};

static void sceneInit(GSPGPU_FramebufferFormats mode);
static void sceneExit(void);
void drawTexture( int x, int y, int width, int height, float left, float right, float top, float bottom);

/* Initialization/Query functions */
static int N3DS_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **N3DS_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *N3DS_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int N3DS_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void N3DS_VideoQuit(_THIS);

/* Hardware surface functions */
static int N3DS_AllocHWSurface(_THIS, SDL_Surface *surface);
static int N3DS_LockHWSurface(_THIS, SDL_Surface *surface);
static void N3DS_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void N3DS_FreeHWSurface(_THIS, SDL_Surface *surface);
static int N3DS_FlipHWSurface (_THIS, SDL_Surface *surface); 

/* etc. */
static void N3DS_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

//Copied from sf2dlib that grabbet it from: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned int next_pow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v >= TEX_MIN_SIZE ? v : TEX_MIN_SIZE;
}

/* DUMMY driver bootstrap functions */

static int N3DS_Available(void)
{
	return(1); //what else?
}

static void N3DS_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *N3DS_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));
	
	pvd = device->hidden; // dirty, but makes some thing faster

	/* Set the function pointers */
	device->VideoInit = N3DS_VideoInit;
	device->ListModes = N3DS_ListModes;
	device->SetVideoMode = N3DS_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = N3DS_SetColors;
	device->UpdateRects = N3DS_UpdateRects;
	device->VideoQuit = N3DS_VideoQuit;
	device->AllocHWSurface = N3DS_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = N3DS_LockHWSurface;
	device->UnlockHWSurface = N3DS_UnlockHWSurface;
	device->FlipHWSurface = N3DS_FlipHWSurface;
	device->FreeHWSurface = N3DS_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = N3DS_InitOSKeymap;
	device->PumpEvents = N3DS_PumpEvents;
//	device->info.blit_hw = 1;

	device->free = N3DS_DeleteDevice;

	return device;
}

VideoBootStrap N3DS_bootstrap = {
	N3DSVID_DRIVER_NAME, "N3ds video driver",
	N3DS_Available, N3DS_CreateDevice
};


int N3DS_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	// Initialize graphics
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	
	vformat->BitsPerPixel = 32;	
	vformat->BytesPerPixel = 4;
	vformat->Rmask = 0xff000000;
	vformat->Gmask = 0x00ff0000;
	vformat->Bmask = 0x0000ff00; 
	vformat->Amask = 0x000000ff; 

	/* We're done! */
	return(0);
}

SDL_Rect **N3DS_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
   	 return (SDL_Rect **) -1;
}

SDL_Surface *N3DS_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{

Uint32 Rmask, Gmask, Bmask, Amask; 
int hw = next_pow2(width);
int hh= next_pow2(height);

	this->hidden->screens = flags & (SDL_DUALSCR); // SDL_DUALSCR = SDL_TOPSCR | SDL_BOTTOMSCR
	if(this->hidden->screens==0) this->hidden->screens = SDL_TOPSCR; //Default
	flags &= ~SDL_DUALSCR;
	flags |= this->hidden->screens;
	
	if(flags & SDL_FULLSCREEN) 
		flags |= (SDL_FITWIDTH | SDL_FITHEIGHT);
	this->hidden->fitscreen = flags & (SDL_FITWIDTH | SDL_FITHEIGHT);

	switch(bpp) {
		case 32:
			Rmask = 0xff000000; 
			Gmask = 0x00ff0000;
			Bmask = 0x0000ff00;
			Amask = 0x000000ff;
			this->hidden->mode=GSP_RGBA8_OES;
			this->hidden->byteperpixel=4;
			break;
		case 24:
			Rmask = 0xff0000; 
			Gmask = 0x00ff00;
			Bmask = 0x0000ff;
			Amask = 0x0;
			this->hidden->mode=GSP_BGR8_OES;
			this->hidden->byteperpixel=3;
			break;
		case 16:
			Rmask = 0b1111100000000000; 
			Gmask = 0b0000011111100000;
			Bmask = 0b0000000000011111;
			Amask = 0x0;
			this->hidden->mode=GSP_RGB565_OES;
			this->hidden->byteperpixel=2;
			break;
		case 15:
			Rmask = 0b1111100000000000; 
			Gmask = 0b0000011111000000;
			Bmask = 0b0000000000111110;
			Amask = 0b1;
			this->hidden->mode=GSP_RGB5_A1_OES;
			this->hidden->byteperpixel=2;
			break;
		default:
			return NULL;
			break;
	}

	if ( this->hidden->buffer ) {
		linearFree( this->hidden->buffer );
	}

	this->hidden->buffer = (u8*) linearAlloc(hw * hh * this->hidden->byteperpixel);
	if ( ! this->hidden->buffer ) {
		SDL_SetError("Couldn't allocate buffer for requested mode");
		return(NULL);
	}


	SDL_memset(this->hidden->buffer, 0, hw * hh * this->hidden->byteperpixel);

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, Rmask, Gmask, Bmask, Amask) ) {
		linearFree(this->hidden->buffer);
		this->hidden->buffer = NULL;
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

	//setup the screens mode
	sceneInit(this->hidden->mode);
	if((flags & SDL_CONSOLETOP) && !(this->hidden->screens & SDL_TOPSCR)) {
		consoleInit(GFX_TOP, NULL);
		this->hidden->console = SDL_CONSOLETOP;
		flags &= ~SDL_CONSOLEBOTTOM;
		// todo here: setup bottom video for graphics;
	} else if((flags & SDL_CONSOLEBOTTOM) && !(this->hidden->screens & SDL_BOTTOMSCR)) {
		consoleInit(GFX_BOTTOM, NULL);
		this->hidden->console = SDL_CONSOLEBOTTOM;
	}
	printf("SDL 1.2.15 for N3DS - Nop90 port v0.4\n");
 	printf("Setting mode %d %dx%d\n", this->hidden->mode, width, height); 

	/* Set up the new mode framebuffer */
	current->flags = SDL_FULLSCREEN | SDL_SWSURFACE| SDL_DOUBLEBUF;
	this->hidden->w = hw;
	this->hidden->h = hh;

	this->hidden->x1 = 0;
	this->hidden->y1 = 0;
	this->hidden->x2 = 0;
	this->hidden->y2 = 0;
	if((this->hidden->screens & SDL_TOPSCR) && (this->hidden->screens & SDL_BOTTOMSCR)){
		this->hidden->w1 = width;
		this->hidden->h1 = height/2;
		this->hidden->w2 = width;
		this->hidden->h2 = height/2;
		this->hidden->y2 = height/2;
	} else {
		this->hidden->w1 = width;
		this->hidden->h1 = height;
		this->hidden->w2 = width;
		this->hidden->h2 = height;
	}

	this->hidden->l1 = 0.0f;
	this->hidden->t1 = 0.0f;
	this->hidden->r1 = (float)this->hidden->w1/(float)this->hidden->w;
	this->hidden->b1 = (float)this->hidden->h1/(float)this->hidden->h;
	this->hidden->l2 = 0.0f;
	this->hidden->t2 = (float)this->hidden->y2/(float)this->hidden->h;
	this->hidden->r2 = (float)this->hidden->w2/(float)this->hidden->w;
	this->hidden->b2 = ((float)this->hidden->y2+(float)this->hidden->h2)/(float)this->hidden->h;

	if((this->hidden->fitscreen & SDL_FITWIDTH)&&(this->hidden->fitscreen & SDL_FITHEIGHT)) {
		this->hidden->scalex= 400.0/(float)this->hidden->w1;
		this->hidden->scaley= 240.0/(float)this->hidden->h1;
	} else if(this->hidden->fitscreen & SDL_FITWIDTH) {
		this->hidden->scalex= 400.0/(float)this->hidden->w1;
		this->hidden->scaley= 1.0f;
	} else 	if(this->hidden->fitscreen & SDL_FITHEIGHT) {
		this->hidden->scaley= 240.0/(float)this->hidden->h1;
		this->hidden->scalex= 1.0f;
	} else {
		this->hidden->scalex= 1.0f;
		this->hidden->scaley= 1.0f;
	}
	this->hidden->scalex2= (400.00f/320.0f)*this->hidden->scalex;
	this->hidden->scaley2= this->hidden->scaley;

	current->w = width;
	current->h = height;
	current->pitch = hw * this->hidden->byteperpixel;
	current->pixels = this->hidden->buffer;

	// Setup the textures
	C3D_TexInit(&spritesheet_tex, hw, hh, this->hidden->mode);
	C3D_TexSetFilter(&spritesheet_tex, GPU_LINEAR, GPU_NEAREST);
	C3D_TexBind(0, &spritesheet_tex);

	/* We're done */
	return(current);
}

/* We don't actually allow hardware surfaces other than the main one */
static int N3DS_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void N3DS_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* We need to wait for vertical retrace on page flipped displays */
static int N3DS_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void N3DS_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static int N3DS_FlipHWSurface (_THIS, SDL_Surface *surface) {
	GSPGPU_FlushDataCache(this->hidden->buffer, this->hidden->w*this->hidden->h*this->hidden->byteperpixel);

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

	C3D_TexBind(0, &spritesheet_tex);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	// Convert image to 3DS tiled texture format
	C3D_SafeDisplayTransfer ((u32*)this->hidden->buffer, GX_BUFFER_DIM(this->hidden->w, this->hidden->h), (u32*)spritesheet_tex.data, GX_BUFFER_DIM(this->hidden->w, this->hidden->h), textureTranferFlags[this->hidden->mode]);
	gspWaitForPPF();
	if (this->hidden->screens & SDL_TOPSCR) {
		C3D_FrameDrawOn(VideoSurface1);
		drawTexture((400-this->hidden->w1*this->hidden->scalex)/2,(240-this->hidden->h1*this->hidden->scaley)/2, this->hidden->w1*this->hidden->scalex, this->hidden->h1*this->hidden->scaley, this->hidden->l1, this->hidden->r1, this->hidden->t1, this->hidden->b1);  
	}
	if (this->hidden->screens & SDL_BOTTOMSCR) {
		C3D_FrameDrawOn(VideoSurface2);
		drawTexture((400-this->hidden->w2*this->hidden->scalex2)/2,(240-this->hidden->h2*this->hidden->scaley2)/2, this->hidden->w2*this->hidden->scalex2, this->hidden->h2*this->hidden->scaley2, this->hidden->l2, this->hidden->r2, this->hidden->t2, this->hidden->b2);  
	}

	C3D_FrameEnd(0);
	// todo: we should flip databuffers here
	return (0);
}

static void N3DS_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{

	if(pvd==NULL) return;
	if(pvd->buffer==NULL) return;
	GSPGPU_FlushDataCache(pvd->buffer, pvd->w*pvd->h*pvd->byteperpixel);

	C3D_FrameBegin(C3D_FRAME_NONBLOCK);

	C3D_TexBind(0, &spritesheet_tex);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	// Convert image to 3DS tiled texture format
	C3D_SafeDisplayTransfer ((u32*)pvd->buffer, GX_BUFFER_DIM(pvd->w, pvd->h), (u32*)spritesheet_tex.data, GX_BUFFER_DIM(pvd->w, pvd->h), textureTranferFlags[pvd->mode]);
	gspWaitForPPF();
	if (pvd->screens & SDL_TOPSCR) {
		C3D_FrameDrawOn(VideoSurface1);
		drawTexture((400-pvd->w1*pvd->scalex)/2,(240-pvd->h1*pvd->scaley)/2, pvd->w1*pvd->scalex, pvd->h1*pvd->scaley, pvd->l1, pvd->r1, pvd->t1, pvd->b1);  
	}
	if (pvd->screens & SDL_BOTTOMSCR) {
		C3D_FrameDrawOn(VideoSurface2);
		drawTexture((400-pvd->w2*pvd->scalex2)/2,(240-pvd->h2*pvd->scaley2)/2, pvd->w2*pvd->scalex2, pvd->h2*pvd->scaley2, pvd->l2, pvd->r2, pvd->t2, pvd->b2);  
	}

	C3D_FrameEnd(0);

}

int N3DS_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* do nothing of note. */
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void N3DS_VideoQuit(_THIS)
{
	if (this->screen->pixels != NULL)
	{
		linearFree(this->screen->pixels);
		this->screen->pixels = NULL;
	}
	sceneExit();
	C3D_Fini();
	gfxExit();
}


//---------------------------------------------------------------------------------
static void sceneInit(GSPGPU_FramebufferFormats mode) {
//---------------------------------------------------------------------------------
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	// Configure attributes for use with the vertex shader
	// Attribute format and element count are ignored in immediate mode
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v2=texcoord

	// Compute the projection matrix
	// Note: we're setting top to 240 here so origin is at top left.
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0, true);
	Mtx_OrthoTilt(&projection2, 0.0, 320.0, 240.0, 0.0, 0.0, 1.0, true);

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);

	if (mode<=1) {
		gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
		gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
	} else	{
		gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
		gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
	}


	// Initialize the top screen render target
	VideoSurface1 = C3D_RenderTargetCreate(240, 400, mode, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(VideoSurface1, C3D_CLEAR_ALL, clearcolors[mode], 0);
	C3D_RenderTargetSetOutput(VideoSurface1, GFX_TOP, GFX_LEFT, displayTranferFlags[mode]);

	// Initialize the bottom screen render target
	VideoSurface2 = C3D_RenderTargetCreate(240, 320, mode, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(VideoSurface2, C3D_CLEAR_ALL, clearcolors[mode], 0);
	C3D_RenderTargetSetOutput(VideoSurface2, GFX_BOTTOM, GFX_LEFT, displayTranferFlags[mode]);

	// Configure the first fragment shading substage to just pass through the texture color
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	// Configure depth test to overwrite pixels with the same depth (needed to draw overlapping sprites)
	C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}

//---------------------------------------------------------------------------------
static void sceneExit(void) {
//---------------------------------------------------------------------------------

	C3D_RenderTargetDelete(VideoSurface1);
	C3D_RenderTargetDelete(VideoSurface2);
	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

//---------------------------------------------------------------------------------
void drawTexture( int x, int y, int width, int height, float left, float right, float top, float bottom) {
//---------------------------------------------------------------------------------

	// Draw a textured quad directly
	C3D_ImmDrawBegin(GPU_TRIANGLES);
		C3D_ImmSendAttrib(x, y, 0.5f, 0.0f); // v0=position
		C3D_ImmSendAttrib( left, top, 0.0f, 0.0f);

		C3D_ImmSendAttrib(x+width, y+height, 0.5f, 0.0f);
		C3D_ImmSendAttrib( right, bottom, 0.0f, 0.0f);

		C3D_ImmSendAttrib(x+width, y, 0.5f, 0.0f);
		C3D_ImmSendAttrib( right, top, 0.0f, 0.0f);

		C3D_ImmSendAttrib(x, y, 0.5f, 0.0f); // v0=position
		C3D_ImmSendAttrib( left, top, 0.0f, 0.0f);

		C3D_ImmSendAttrib(x, y+height, 0.5f, 0.0f);
		C3D_ImmSendAttrib( left, bottom, 0.0f, 0.0f);

		C3D_ImmSendAttrib(x+width, y+height, 0.5f, 0.0f);
		C3D_ImmSendAttrib( right, bottom, 0.0f, 0.0f);
	C3D_ImmDrawEnd();

}

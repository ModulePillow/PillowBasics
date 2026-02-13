#include "Core/Mesh.h"
#include "DirectXMath-apr2025/DirectXMath.h"
#include "Core/Auxiliaries.h"
#include "Core/Texture.h"

#include "OpenAL-1.24.3/al.h"
#include "OpenAL-1.24.3/alc.h"
#include "PhysX-4.1/PxPhysicsAPI.h"
#include <iostream>
using namespace physx;

// 简单的错误回调类
class SimpleErrorCallback : public PxErrorCallback {
public:
   void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
      std::cerr << "PhysX Error: " << message << " in " << file << " at line " << line << std::endl;
   }
};

void TempCode()
{
   //SetWindowMode(true);
   //D3D12Renderer renderer(windowHandle, 2);
   //
   //
   //using namespace DirectX;
   //XMVECTOR v = XMVectorSet(1, 1, 1, 1);
   //XMVECTOR v2 = XMVector4Dot(v, v);
   //float result;
   //XMStoreFloat(&result, v2);
   //bool SSE4Check = SSE4::XMVerifySSE4Support();
   //LoadTexture(L"Textures\\SRGBInterpolationExample.png");

   //ALCdevice* device = alcOpenDevice(NULL);
   //ALCcontext* context = alcCreateContext(device, NULL);
   //alcMakeContextCurrent(context);

   //// Genrate sin wave
   //#define SAMPLE_RATE 44100
   //#define FREQUENCY 440.0f
   //#define DURATION 3.0f
   //int samples = (int)(SAMPLE_RATE * DURATION);
   //short* bufferData = (short*)malloc(samples * sizeof(short));
   //for (int i = 0; i < samples; ++i) {
   //   bufferData[i] = (short)(32760.0f * sinf(2.0f * 3.14 * FREQUENCY * i / SAMPLE_RATE));
   //}

   //// Create buffer
   //ALuint buffer;
   //alGenBuffers(1, &buffer);
   //alBufferData(buffer, AL_FORMAT_MONO16, bufferData, samples * sizeof(short), SAMPLE_RATE);

   //// Create audio source
   //ALuint source;
   //alGenSources(1, &source);
   //alSourcei(source, AL_BUFFER, buffer);

   //// Play audio
   //alSourcePlay(source);
   //printf("播放 440 Hz 正弦波...\n");

   //// wait to finish
   //ALint state;
   //do {
   //   alGetSourcei(source, AL_SOURCE_STATE, &state);
   //} while (state == AL_PLAYING);

   //// Release
   //alDeleteSources(1, &source);
   //alDeleteBuffers(1, &buffer);
   //free(bufferData);
   //alcMakeContextCurrent(NULL);
   //alcDestroyContext(context);
   //alcCloseDevice(device);

   //static PxDefaultAllocator allocator;
   //static SimpleErrorCallback errorCallback;
   //PxFoundation* foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
   //if (!foundation) {
   //   std::cerr << "Failed to create PhysX Foundation!" << std::endl;
   //   //return 1;
   //}

   //// Initialize PhysX SDK
   //PxPhysics* physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale());
   //if (!physics) {
   //   std::cerr << "Failed to create PhysX SDK!" << std::endl;
   //   foundation->release();
   //   //return 1;
   //}

   //// Create a simple scene
   //PxSceneDesc sceneDesc(physics->getTolerancesScale());
   //sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
   //PxDefaultCpuDispatcher* dispatcher = PxDefaultCpuDispatcherCreate(1);
   //sceneDesc.cpuDispatcher = dispatcher;
   //sceneDesc.filterShader = PxDefaultSimulationFilterShader;

   //PxScene* scene = physics->createScene(sceneDesc);
   //if (!scene) {
   //   std::cerr << "Failed to create PhysX Scene!" << std::endl;
   //   dispatcher->release();
   //   physics->release();
   //   foundation->release();
   //   //return 1;
   //}

   //// Output
   //std::cout << "PhysX initialized successfully! Scene created." << std::endl;

   //// Release
   //scene->release();
   //dispatcher->release();
   //physics->release();
   //foundation->release();
}
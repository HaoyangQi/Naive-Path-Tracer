//
// Created by goksu on 2/25/20.
//
#include "Scene.hpp"

#pragma once
struct hit_payload
{
    float tNear;
    uint32_t index;
    Vector2f uv;
    Object* hit_obj;
};

class Renderer
{
public:
    void Render(const Scene& scene);

private:
};

#include <thread>
#include <mutex>
#include <chrono>

class RendererMT
{
public:
    RendererMT(const Scene& s) : 
        i(0), j(0), cntComplete(0), scene(s), spp(16), nThread(std::thread::hardware_concurrency()) {}
    void Render();
    void setSPP(int n) { spp = n; }
    void setRenderThreadCount(int n) { nThread = n; }

private:
    void RenderKernel();
    void RenderInfo();

private:
    int spp, nThread;
    const Scene& scene;
    std::vector<Vector3f> framebuffer;
    uint32_t i, j, cntComplete;
    std::mutex dataMutex;
};

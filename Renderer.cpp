//
// Created by goksu on 2/25/20.
//

#include <fstream>
#include "Scene.hpp"
#include "Renderer.hpp"


inline float deg2rad(const float& deg) { return deg * M_PI / 180.0; }

const float EPSILON = 0.00001;

// The main render function. This where we iterate over all pixels in the image,
// generate primary rays and cast these rays into the scene. The content of the
// framebuffer is saved to a file.
void Renderer::Render(const Scene& scene)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    float scale = tan(deg2rad(scene.fov * 0.5));
    float imageAspectRatio = scene.width / (float)scene.height;
    Vector3f eye_pos(278, 273, -800);
    int m = 0;

    // change the spp value to change sample ammount
    int spp = 16;
    std::cout << "SPP: " << spp << "\n";
    for (uint32_t j = 0; j < scene.height; ++j) {
        for (uint32_t i = 0; i < scene.width; ++i) {
            // generate primary ray direction
            float x = (2 * (i + 0.5) / (float)scene.width - 1) *
                      imageAspectRatio * scale;
            float y = (1 - 2 * (j + 0.5) / (float)scene.height) * scale;
            
            Vector3f dir = normalize(Vector3f(-x, y, 1));
            for (int k = 0; k < spp; k++){
                framebuffer[m] += scene.castRay(Ray(eye_pos, dir), 0) / spp;  
            }
            m++;
        }
        UpdateProgress(j / (float)scene.height);
    }
    UpdateProgress(1.f);

    // save framebuffer to file
    FILE* fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i) {
        static unsigned char color[3];
        color[0] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].x), 0.6f));
        color[1] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].y), 0.6f));
        color[2] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].z), 0.6f));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);    
}

void RendererMT::Render()
{
    framebuffer.reserve(scene.width * scene.height);

    // show some info
    std::cout << "SPP: " << spp << "\n";
    std::cout << "Hardware concurrancy supported: " 
        << std::thread::hardware_concurrency() << "\n";
    std::cout << "Total number of render threads: " << nThread << "\n";

    // Spawn threads
    std::vector<std::thread> renderThreads(nThread + 1);
    renderThreads[0] = std::thread(&RendererMT::RenderInfo, this);
    for (int idx = 1; idx < renderThreads.size(); idx++)
    {
        renderThreads[idx] = std::thread(&RendererMT::RenderKernel, this);
    }
    // Join Threads
    for (int idx = 0; idx < renderThreads.size(); idx++)
    {
        renderThreads[idx].join();
    }
    UpdateProgress(1.f);

    // save framebuffer to file
    FILE* fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i) {
        static unsigned char color[3];
        color[0] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].x), 0.6f));
        color[1] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].y), 0.6f));
        color[2] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].z), 0.6f));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);
}

void RendererMT::RenderKernel()
{
    float x, y;
    int m;

    int w = scene.width;
    int h = scene.height;
    float scale = tan(deg2rad(scene.fov * 0.5));
    float imageAspectRatio = w / (float)h;
    Vector3f eye_pos(278, 273, -800);

    while (true)
    {
        // CRITICAL SECTION: fetch i,j and update them
        dataMutex.lock();
        if (j >= h)
        {
            dataMutex.unlock();
            break;
        }
        else
        {
            x = (2 * (i + 0.5) / (float)w - 1) *
                imageAspectRatio * scale;
            y = (1 - 2 * (j + 0.5) / (float)h) * scale;
            m = i + w * j;

            // update i, j
            j = (i + 1 >= w) ? j + 1 : j;
            i = (i + 1) % w;
        }
        dataMutex.unlock();

        // Trace
        Vector3f dir = normalize(Vector3f(-x, y, 1));
        for (int k = 0; k < spp; k++) {
            framebuffer[m] += scene.castRay(Ray(eye_pos, dir), 0) / spp;
        }

        // CRITICAL SECTION: update progress data
        dataMutex.lock();
        cntComplete++;
        dataMutex.unlock();
    }
}

void RendererMT::RenderInfo()
{
    int total = scene.width * scene.height;

    while (cntComplete != total)
    {
        // CRITICAL SECTION: update info
        dataMutex.lock();
        UpdateProgress(cntComplete / (float)total);
        dataMutex.unlock();

        // Sleep for some time to avoid IO overhead
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

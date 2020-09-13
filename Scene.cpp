//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"


void Scene::buildBVH() {
    printf(" - Generating BVH...\n\n");
    this->bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}

void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    float emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            emit_area_sum += objects[k]->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            emit_area_sum += objects[k]->getArea();
            if (p <= emit_area_sum){
                objects[k]->Sample(pos, pdf);
                break;
            }
        }
    }
}

bool Scene::trace(
        const Ray &ray,
        const std::vector<Object*> &objects,
        float &tNear, uint32_t &index, Object **hitObject)
{
    *hitObject = nullptr;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (objects[k]->intersect(ray, tNearK, indexK) && tNearK < tNear) {
            *hitObject = objects[k];
            tNear = tNearK;
            index = indexK;
        }
    }


    return (*hitObject != nullptr);
}

// Implementation of Path Tracing
Vector3f Scene::castRay(const Ray &ray, int depth) const
{
    Vector3f L_dir(0.0f), L_indir(0.0f);
    Intersection p, q, sample_light, blocked;
    Vector3f wi, ws, wo, N, NN;
    float pdf_light, dist;
    
    // find the point-of-intersection of the ray
    p = intersect(ray);
    N = p.normal;
    wo = -ray.direction;

    if (p.happened)
    {
        // sample light to calculate direct illumination
        // direct illumination does not require RR
        sampleLight(sample_light, pdf_light);
        ws = sample_light.coords - p.coords;
        NN = sample_light.normal;
        dist = ws.norm();
        ws = normalize(ws);
        blocked = intersect(Ray(p.coords, ws));
        
        if (depth == 0 && p.obj->hasEmit())
        {
            // if directly hit light source:
            // N == NN, p and sample_light on light surface
            // so ws on light surface, and dot(ws,N) and dot(ws,NN) are 0
            // In this case, color of light emission is the exact color
            L_dir = sample_light.emit;
        }
        else if (dist - (blocked.coords - p.coords).norm() <= 0.001f)
        {
            // if not blocked, direct illumination is valid
            // blocked.distance is t value, NOT distance!
            // threshold normally is 1e-4 (this case we use 1e-3)
            // too small threshold will result some torn effect eg. black lines on back wall
            // EPSILON is 1e-5, which is too small
            // the expression is to compare float-type distance
            L_dir = sample_light.emit * p.m->eval(wo, ws, N) *
                dotProduct(ws, N) * dotProduct(-ws, NN) /
                pow(dist, 2) / pdf_light;
        }

        // Now trace reflectance (RR required)
        if (get_random_float() < RussianRoulette)
        {
            wi = normalize(p.m->sample(wo, N));
            q = intersect(Ray(p.coords, wi));
            // if intersect with an non-emitting object
            if (q.happened && !q.obj->hasEmit())
            {
                L_indir = castRay(Ray(p.coords, wi), depth + 1) *
                    p.m->eval(wo, wi, N) * dotProduct(wi, N) /
                    p.m->pdf(wo, wi, N) / RussianRoulette;
            }
        }
    }
    
    return L_dir + L_indir;
}
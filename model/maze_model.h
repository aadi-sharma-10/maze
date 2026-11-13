/* maze_model.h - made by tools/train_model.py, don't edit by hand.
   feature order: vertices, edges, dead_ends, junctions, loops, shortest_path, detour_ratio */
#ifndef MAZE_MODEL_H
#define MAZE_MODEL_H

#define MODEL_FEATURE_COUNT 7

static const double model_mean[7] = {251.33333333333334, 253.33333333333334, 13.139583333333333, 16.737500000000001, 3, 93.974999999999994, 3.0269969258651108};
static const double model_scale[7] = {145.72653232072128, 143.57653785428252, 8.4754114841929002, 4.5709510771829525, 2.1602468994692869, 74.421318910197598, 1.5458292770151265};
static const double model_weights[7] = {-0.049896642161265423, -0.050450123801978788, -0.043685458724242426, 0.041554657476921206, 0.012873772353166786, 0.00027871100772336455, -0.090918861380014038};
static const double model_intercept = 0.31350911458333336;

/* predicted success rate (0 to 1) for the two bots */
static double predict_success(const double features[MODEL_FEATURE_COUNT])
{
    double score = model_intercept;
    int i;
    for (i = 0; i < MODEL_FEATURE_COUNT; i++)
        score += model_weights[i] * ((features[i] - model_mean[i]) / model_scale[i]);
    if (score < 0.0)
        score = 0.0;
    if (score > 1.0)
        score = 1.0;
    return score;
}

#endif

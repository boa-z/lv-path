/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 boa-z
 * Independent double Bernstein/dense-chord oracle. Large static arrays are
 * host-test storage only; no oracle or double arithmetic enters the core. */
#include "test_util.h"
#include "../src/pg_internal.h"
#include <string.h>

enum { REF_STEPS = 65536, CAPACITY = 16385 };
typedef struct { double x, y; } point64_t;
static double lengths[REF_STEPS + 1];
static pg_measure_sample_t samples[CAPACITY];
static double max_ratio;

static point64_t reference(const pg_point_t *p, unsigned degree, double t) {
    double u = 1.0 - t, w[4];
    point64_t q = {0, 0};
    unsigned i;
    if (degree == 2u) {
        w[0]=u*u; w[1]=2*u*t; w[2]=t*t; w[3]=0;
    } else {
        w[0]=u*u*u; w[1]=3*u*u*t; w[2]=3*u*t*t; w[3]=t*t*t;
    }
    for (i=0; i<=degree; ++i) { q.x+=w[i]*p[i].x; q.y+=w[i]*p[i].y; }
    return q;
}
static void integrate(const pg_point_t *p, unsigned degree) {
    point64_t previous = reference(p, degree, 0);
    unsigned i;
    lengths[0]=0;
    for (i=1; i<=REF_STEPS; ++i) {
        point64_t q=reference(p, degree, (double)i/REF_STEPS);
        lengths[i]=lengths[i-1]+hypot(q.x-previous.x, q.y-previous.y);
        previous=q;
    }
}
static double arc_at(double t) {
    double index=t*REF_STEPS;
    unsigned lo=(unsigned)index;
    if (lo>=REF_STEPS) return lengths[REF_STEPS];
    return lengths[lo]+(lengths[lo+1]-lengths[lo])*(index-lo);
}
static double parameter_at(double d) {
    unsigned lo=0, hi=REF_STEPS;
    if (d<=0) return 0;
    if (d>=lengths[REF_STEPS]) return 1;
    while (hi-lo>1) {
        unsigned mid=lo+(hi-lo)/2;
        if (lengths[mid]<=d) lo=mid; else hi=mid;
    }
    return (lo+(d-lengths[lo])/(lengths[hi]-lengths[lo]))/REF_STEPS;
}
static void check_curve(const char *name, const pg_point_t *p, unsigned degree,
                         float tolerance, double numerical_allowance) {
    pg_cmd_t commands[2]={PG_MOVE_TO(0,0), PG_LINE_TO(0,0)};
    pg_path_t path={commands,2};
    pg_measure_t measure;
    pg_result_t result;
    double tau=fmax(tolerance,PG_MIN_TOLERANCE), worst=0;
    unsigned i;
    commands[0].p1=p[0];
    commands[1].type=degree==2u ? PG_CMD_QUAD : PG_CMD_CUBIC;
    commands[1].p1=p[1]; commands[1].p2=p[2]; commands[1].p3=p[3];
    integrate(p,degree);
    result=pg_measure_init(&measure,&path,samples,CAPACITY,tolerance);
    if (result!=PG_OK) printf("accuracy: %s init=%s\n",name,pg_result_str(result));
    TU_EXPECT(result==PG_OK);
    if (result!=PG_OK) return;
    TU_NEAR(measure.total_length,lengths[REF_STEPS],tau/4+numerical_allowance);
    for (i=0; i<measure.sample_count; ++i) {
        TU_NEAR(samples[i].distance,arc_at(samples[i].t),tau/4+numerical_allowance);
        if (i) TU_EXPECT(samples[i].distance>samples[i-1].distance);
    }
    for (i=0; i<=512; ++i) {
        float fraction=(float)i/512, d=fraction*measure.total_length;
        pg_point_t q,tangent;
        pg_locate_t located;
        point64_t expected;
        double residual;
        /* Arc residual catches incorrect branches at self-intersections. */
        pg_measure_locate(&measure,d,&located);
        residual=fabs(arc_at(located.t)-d);
        TU_EXPECT(residual<=tau*0.75+numerical_allowance);
        if (residual>worst) worst=residual;
        TU_EXPECT(pg_measure_get_pos_tan(&measure,d,&q,&tangent)==PG_OK);
        expected=reference(p,degree,parameter_at(d));
        TU_EXPECT(hypot(q.x-expected.x,q.y-expected.y)<=tau*0.75+numerical_allowance);
        TU_NEAR(hypot(tangent.x,tangent.y),1,2e-6);
        TU_EXPECT(pg_measure_get_pos_tan_normalized(&measure,fraction,&q,NULL)==PG_OK);
        expected=reference(p,degree,parameter_at(fraction*lengths[REF_STEPS]));
        TU_EXPECT(hypot(q.x-expected.x,q.y-expected.y)<=tau+numerical_allowance);
    }
    if (worst/tau>max_ratio) max_ratio=worst/tau;
    printf("accuracy: %-20s samples=%u max_arc_residual=%.9g tau=%.9g\n",
           name,measure.sample_count,worst,tau);
}
static void check_path_budget(void) {
    enum { COUNT=16 };
    const pg_point_t base[4]={{0,0},{50,80},{100,0},{0,0}};
    pg_cmd_t commands[COUNT+1]={PG_MOVE_TO(0,0)};
    pg_path_t path={commands,COUNT+1};
    pg_measure_t measure;
    pg_result_t result;
    const float tau=0.05f;
    unsigned i;
    integrate(base,2);
    for (i=1; i<=COUNT; ++i)
        commands[i]=(pg_cmd_t)PG_QUAD_TO((i-1)*100+50,80,i*100,0);
    result=pg_measure_init(&measure,&path,samples,CAPACITY,tau);
    TU_EXPECT(result==PG_OK);
    if (result!=PG_OK) return;
    TU_NEAR(measure.total_length,COUNT*lengths[REF_STEPS],tau/4+0.002);
    for (i=0; i<measure.sample_count; ++i) {
        double s=(samples[i].command_index-1)*lengths[REF_STEPS]+arc_at(samples[i].t);
        TU_NEAR(samples[i].distance,s,tau/4+0.002);
    }
    for (i=0; i<=1000; ++i) {
        float d=measure.total_length*(float)i/1000;
        pg_locate_t located;
        double s;
        pg_measure_locate(&measure,d,&located);
        s=(located.command_index-1)*lengths[REF_STEPS]+arc_at(located.t);
        TU_NEAR(s,d,tau*0.75+0.002);
    }
}
static void check_failures_and_slice(void) {
    const pg_cmd_t commands[]={PG_MOVE_TO(0,0),PG_QUAD_TO(0,0,100,0)};
    const pg_cmd_t huge[]={PG_MOVE_TO(0,0),PG_QUAD_TO(0,0,1e8f,0)};
    const pg_path_t path={commands,2},difficult={huge,2};
    pg_measure_t measure;
    pg_cmd_t output[4];
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;
    pg_point_t point;
    TU_EXPECT(pg_measure_init(&measure,&path,samples,8,0.0001f)==PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(measure.path==NULL && measure.samples==NULL && measure.sample_count==0);
    TU_EXPECT(pg_measure_get_pos_tan(&measure,50,&point,NULL)==PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_init(&measure,&difficult,samples,CAPACITY,0.0001f)==PG_ERR_TOLERANCE_NOT_MET);
    TU_EXPECT(measure.path==NULL && measure.samples==NULL && measure.total_length==0);
    TU_EXPECT(strcmp(pg_result_str(PG_ERR_TOLERANCE_NOT_MET),"PG_ERR_TOLERANCE_NOT_MET")==0);
    TU_EXPECT(pg_measure_init(&measure,&path,samples,CAPACITY,0.0001f)==PG_OK);
    TU_EXPECT(pg_path_buffer_init(&buffer,output,4)==PG_OK);
    writer=pg_path_buffer_writer(&buffer);
    TU_EXPECT(pg_measure_slice(&measure,25,75,&writer)==PG_OK);
    TU_EXPECT(buffer.count==2 && output[1].type==PG_CMD_QUAD);
    TU_POINT_NEAR(output[0].p1,25,0,0.0001);
    TU_POINT_NEAR(output[1].p2,75,0,0.0001);
}
int main(void) {
    static const struct { const char *name; unsigned degree; pg_point_t p[4]; } cases[]={
        {"straight quadratic",2,{{0,0},{0,0},{100,0},{0,0}}},
        {"quadratic arch",2,{{0,0},{50,120},{100,0},{0,0}}},
        {"quadratic reversal",2,{{0,0},{100,0},{10,0},{0,0}}},
        {"quadratic return",2,{{0,0},{100,100},{0,0},{0,0}}},
        {"straight cubic",3,{{0,0},{0,0},{0,0},{100,0}}},
        {"cubic endpoint cusp",3,{{0,0},{0,0},{0,100},{100,100}}},
        {"cubic interior cusp",3,{{-1,1},{1,-1.0f/3},{-1,-1.0f/3},{1,1}}},
        {"cubic retracing",3,{{0,0},{100,0},{-100,0},{0,0}}},
        {"cubic closed loop",3,{{0,0},{150,200},{-150,200},{0,0}}},
        {"cubic crossing",3,{{0,0},{200,200},{-100,200},{100,0}}}
    };
    unsigned i;
    for (i=0; i<PG_ARRAY_SIZE(cases); ++i) {
        check_curve(cases[i].name,cases[i].p,cases[i].degree,0.01f,0.0001);
        check_curve(cases[i].name,cases[i].p,cases[i].degree,0.001f,0.0001);
    }
    {
        const pg_point_t small[4]={{0,0},{0,0},{0,0},{0.001f,0}};
        const pg_point_t large[4]={{0,0},{0,1e8f},{1e8f,1e8f},{1e8f,0}};
        const pg_point_t offset[4]={{1e5f,1e5f},{1e5f,1e5f+80},{1e5f+120,1e5f+80},{1e5f+120,1e5f}};
        check_curve("small / clamped tau",small,3,1e-8f,1e-8);
        check_curve("large coordinates",large,3,1e4f,100);
        check_curve("translated curve",offset,3,1,0.1);
    }
    check_path_budget();
    check_failures_and_slice();
    printf("accuracy: max_arc_residual/tau=%.6f (tested corpus)\n",max_ratio);
    return TU_SUMMARY() ? 1 : 0;
}

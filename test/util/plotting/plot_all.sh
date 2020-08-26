#!/bin/bash

DATA_DIR=/home/kavon/research/papers/private-repo/dissertation/data
TORMENTA_DATA=${DATA_DIR}/tormenta-8-24.csv
DONNER_DATA=${DATA_DIR}/donner-8-24.csv
ARMENTA_DATA=${DATA_DIR}/armenta-8-24.csv

PAPER_FIGURE_DIR=/home/kavon/research/papers/private-repo/dissertation/figures
HALO_ROOT_DIR=/home/kavon/phd/halo

PLOT_PY=${HALO_ROOT_DIR}/test/util/plotting/plot.py

####################
# PLOT

# performance plots
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/tormenta --exclude="default,halomon,halo-ipc" ${TORMENTA_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/donner --exclude="default,halomon,halo-ipc" ${DONNER_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/armenta --exclude="default,halomon,halo-ipc" ${ARMENTA_DATA}

# metric comparison plots
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/tormenta-metric --palette="colorblind" --baseline="jit" --exclude="default,halomon,aot" ${TORMENTA_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/donner-metric --palette="colorblind" --baseline="jit" --exclude="default,halomon,aot" ${DONNER_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/armenta-metric --palette="colorblind" --baseline="jit" --exclude="default,halomon,aot" ${ARMENTA_DATA}

# overhead plots
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/tormenta --palette="BuGn_r" --bars=true --baseline="default" --exclude="aot,jit,halo-calls,halo-ipc" ${TORMENTA_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/donner --palette="BuGn_r" --bars=true --baseline="default" --exclude="aot,jit,halo-calls,halo-ipc" ${DONNER_DATA}
${PLOT_PY} --dir=${PAPER_FIGURE_DIR}/armenta --palette="BuGn_r" --bars=true --baseline="default" --exclude="aot,jit,halo-calls,halo-ipc" ${ARMENTA_DATA}
import array, random, itertools
import deap.tools
import numpy as np

from genetic import GeneticOperators

def genetic(symbolic, Template, compute_perf, perf_metric, out):
    GA = GeneticOperators(symbolic, Template, out)
    return GA.optimize(maxtime='2m30s', maxgen=1000, compute_perf=compute_perf, perf_metric=perf_metric)
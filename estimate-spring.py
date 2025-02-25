from estimator import *
from estimator.nd import NoiseDistribution
from math import sqrt, ceil, floor

all_spring_q={257, 514}
all_spring_n={128, 256, 514}

for spring_q in all_spring_q:
    for spring_n in all_spring_n:
        print('q='+str(spring_q)+' n='+str(spring_n))
        lwe=LWE.Parameters(n=spring_n, q=spring_q, tag="Spring-BCH",
                Xs=NoiseDistribution.UniformMod(spring_q),Xe=NoiseDistribution.UniformMod((spring_q//2)))
        out=LWE.estimate(lwe)
        print(out)

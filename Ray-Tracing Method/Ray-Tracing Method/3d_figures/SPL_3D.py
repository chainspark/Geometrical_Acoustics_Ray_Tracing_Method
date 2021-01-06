import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

data = np.loadtxt('Case3D_.025_one_wall_250Hz_1mil_100db_cube_analytical_complexR_1_0.csv',delimiter=',',dtype=None)

X,Y,SPL,number_detected,analytical=[],[],[],[],[]

for d in data[:,:] :
   X.append(d[0])
   Y.append(d[1])
   SPL.append(d[2])
   number_detected.append(d[3])


fig = plt.figure()
ax = Axes3D(fig)

surf = ax.plot_trisurf(X, Y, SPL, cmap=cm.magma,linewidth=0.1)
fig.colorbar(surf, shrink=0.5, aspect=5)
plt.xlabel("X Axis")
plt.ylabel("Y Axis")
plt.title("500 Hz ")


plt.show()
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import random

#dt = np.dtype([('starttime', np.float32), ('amplitude', np.float32), ('endtime', np.float32)])

def plotSignal(starttime, amplitude, endtime):
    X = np.zeros((1))
    Y = np.zeros((1))
    s = max(endtime)
    k = 1;
    i = 0;
    a = 0;
    while k <= s:
        if k == starttime[i]:
            while k < endtime[i]:
                X = np.append(X, k)
                Y = np.append(Y, amplitude[i])
                k = k + 1
            #end while
            i = i+1
        else:
            X = np.append(X, k)
            Y = np.append(Y, a)
            k = k+1
        #end if
    #end while
    #rect = [0, max(X), 0, max(Y)+2]
    plt.plot(X, Y)
    plt.xlim(0, max(endtime))
    plt.ylim(0, max(amplitude)+2)
    plt.show()
#end plot
import numpy as np
import matplotlib.pyplot as plt
from sklearn import svm

# Add a +1 point above to break intersection
X = np.array([
    [0, 0],    # x1, +1
    [4, 0],    # x2, -1
    [2, 10],   # x3, +1
    [3, 8],    # x4, -1
    [1.5, 11]  # tweak: extra +1
])
y = np.array([1, -1, 1, -1, 1])

clf = svm.SVC(kernel='linear', C=1e5)
clf.fit(X, y)

w = clf.coef_[0]
b = clf.intercept_[0]
print(f"Hyperplane formula: {w[0]:.3f}*x + {w[1]:.3f}*y + {b:.3f} = 0")

xx = np.linspace(-1,5,100)
yy = -(w[0]*xx + b)/w[1]

plt.figure(figsize=(6,6))
plt.scatter(X[y==1,0], X[y==1,1], color='blue', s=100, label='Class +1')
plt.scatter(X[y==-1,0], X[y==-1,1], color='red', s=100, label='Class -1')
plt.plot([X[0,0], X[1,0]], [X[0,1], X[1,1]], 'k--', lw=2, label='Line x1-x2')
plt.plot(xx, yy, 'g-', lw=2, label='SVM hyperplane')
plt.scatter(clf.support_vectors_[:,0], clf.support_vectors_[:,1], s=200,
            facecolors='none', edgecolors='k', label='Support Vectors')
plt.xlim(-1,5)
plt.ylim(-1,12)
plt.xlabel('X'); plt.ylabel('Y'); plt.grid(True)
plt.legend()
plt.title('intersection disappears')
plt.show()

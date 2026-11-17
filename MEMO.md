[toc]

> 这份备忘写于第一版 FMU 迭代（`fmi2-regressor-cpp`）。代码路径和程序结构已按当前仓库更新，结论未改。

# 关于制作`FMU`的思考

第一次接触`FMU`这个东西是26年7月底, 在此之前对于`FMU`的理解大概就是一个计算模块, 类似`Simulink`中的一个block。但实际开始制作之后, 发现比想象的要复杂一些。一个基本的`FMU`会有如下文件夹(以`FMI2.0`为例):

```txt
├─binaries/
│  └─win64
├─resources/
│  └─xxx
└─modelDescription.xml
```

放在`binaries`下的`dll`是最好理解的部分, 需要根据`FMI`的标准, 去实现一堆接口。其中有几个接口很形象, 我放在这加强认知:

```cpp
fmi2Component fmi2Instantiate(
    fmi2String instanceName,
    fmi2Type fmuType,
    fmi2String fmuGUID,
    fmi2String fmuResourceLocation,
    const fmi2CallbackFunctions* functions,
    fmi2Boolean visible,
    fmi2Boolean loggingOn
) // 这个是用来初始化模型的, 返回的 `fmi2Component` 本质上是一个 `void *`, 

fmi2Status fmi2DoStep(
    fmi2Component c,
    fmi2Real currentCommunicationPoint,
    fmi2Real communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint
) // 这个是用来时间步进迭代的, 用于Co-Simulation中内部自行实现求解器的场景
```

按照规定的格式去实现一堆这样的接口, 那么最终`FMU`的动态链接库的对应代码就制作完成了。

但是问题也就出在动态链接库这里——很多时候就受制于平台、以及平台对应的动态链接库。尤其是`cpp`版本高了后, 需要手工指定一些标准库要打包进`dll`中, 很容易踩坑。另外如果涉及到外部的库调用比如`onnx`, 那么情形还会更加复杂。还需要寻找对应平台的`runtime`给一起打包。

不过不管怎么说, 只要指定了`resources`下资源文件的排布方式, 在对应`cpp`代码对应接口中来载入资源并初始化模型, 肯定是可行的。

# 关于冷与热的抉择

这里的冷、热是指在制作`FMU`的过程中, 我是考虑去现编码并编译一个`FMU`还是把既有的动态链接库给引入进来。因为在观测前同事代码的过程中, 我注意到采用的技术栈是在`python`程序中去改了一个 template 的`cpp`模板, 填写其对应的参数信息, 再把对应的`cpp`文件移入一个含有`cmake`的文件夹中, 然后走`cmake`编译——这很酷。

但是再仔细想完这个问题后, 我注意到两件事情:

1. 如果程序部署在 X 平台下, 则其只能部署生成 X 平台下的`FMU`。比如常见的服务端在`Linux`服务器下, 但是要生成的是`Windows`下的`FMU`, 这个时候就很麻烦了, 可能会用到虚拟机之类的隔靴搔痒的技术栈, 破坏热编码编译的这条技术栈。
2. 热编码、编译还是没有解决最根本的"算法要重写"这条路线, 一旦算法有比较大的变动几乎是要重写一份`cpp`的。并没有起到"一份模板通行天下"的作用。

所以最后我个人觉得还是需要将`FMU`生成与`DLL`生成这两件事情给**分割**开来。也就是冷处理整个`FMU`打包的过程。`DLL`生成过程应该完全独立开来, 需要的参数从`resources`去取。

# 关于程序设计

毫无疑问对每个算法去全部重写一套接口还是太麻烦了, 于是我考虑的设计还是写一个`class`, 预先写好所有接口的实现, 让算法类只需要关心"怎么算"。

第一版是让算法类继承一个`BaseModel`、用虚函数去`override`需要重写的接口。现在改成了组合:`include/fmi2/`和`include/fmi3/`里的基类只负责 FMI 生命周期, 算法本身写成一个不碰任何 FMI 类型、也不碰回调的普通类, 由`src/model/detail/fmi2_cs_model.hpp`和`fmi3_cs_model.hpp`两个适配器把它套进各自的协议里。这样同一份算法可以同时给 FMI 2 和 FMI 3 用, 换协议不用动算法, 换算法也不用动协议。

这里额外说明一下虚函数的性能问题: 因为`FMU`说白了本身也不是为高性能而设计的, 从`SetReal`, `GetReal`这样类似的变量副本接口就能看出来, 在仿真过程中会有不必要的、频繁的数据互拷。那么虚函数在整个流程中导致的那一点性能损失也可以忽略不计了——`FMU`是一个工程性质的实现, 不必苛求性能。

这里建议考虑的阅读代码的顺序是:

1. 先读`src/core/fmi2/fmi2_wrapper.cpp`, 这是对外暴露的`FMI2.0`必须的接口;
2. 再读`include/fmi2/fmi2_base.hpp`和`include/fmi2/cs_base.hpp`, 这里是所有接口的基本实现, 也就是说具体模型里不实现的话就默认走这里的实现;
3. 再读`src/model/detail/fmi2_cs_model.hpp`, 它把算法接进 FMI 2 的生命周期和 C API 数据转换;
4. 再读任意一个具体的模型实现, 比如`src/model/vector_coder_onnx/model.hpp`, 展示了从读取资源、入参、求值、出参、重置等一系列流程; `window_pure_onnx` 那个则是带滚动窗口的变体;
5. 如果模型需要用到`onnx`, 则需要额外看一下`include/onnxengine/`, 这里展示了如何调取对应`onnx`运行时动态链接库来催动`onnx`格式运作的。

整个代码仓库的产物是编译平台对应的动态链接库, 可以用根目录下`pack.py`脚本抓取产物至`artifacts/`目录。再把动态链接库交给打包`FMU`的`python`程序消费即可。

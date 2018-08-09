# android_app_update
安卓客户端和服务端实现增量更新全解


## 1.下载bsdiff库

首先增量更新用到了开源的bsdiff库，先到官网下载，地址是http://www.daemonology.net/bsdiff/  。但是目前官网上的window port连接失效了，不知道原因，我只能百度去下载 bsdiff4.3-win32-src.zip。

## 2.新建一个vs2017的空项目，名字叫bsdiff，拆分安装包的工程。把工程里的.h和.c文件分别导入进去。

![](https://upload-images.jianshu.io/upload_images/2587882-55644bd30b8b7780.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/674)

导入后有很多编译错误

![](https://upload-images.jianshu.io/upload_images/2587882-ec691c6033172924.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

原因是用了一些不安全的函数，如fopen，需要声明一些宏定义，在每个报错的c文件最前端加入这样的声明 #define _CRT_SECURE_NO_WARNINGS，还有将setmode改成_setmode，fileno改成_fileno，将isatty改成_isatty，将lseek改成_lseek，将read改成_read。至此工程编译全部通过。

最后设置打包成dll动态库。



## 3.新建java工程，作为服务端的拆分的程序

编写native函数，

private native static int diff(String oldFile, String newFile, String patchFile);

参数设置有三个：旧的apk路径，新的apk路径，以及新旧apk拆分出来的拆分包路径。

然后用javah命令来生成头文件，可以一键配置，具体参考这篇文章。https://blog.csdn.net/wisevenus/article/details/53046076

把生成的头文件放在vs的工程目录下，由于需要jni.h，和jni_md.h 这两个头文件，需要去C:\Program Files\Java\jdk1.8.0_181\include里面复制出来放在工程目录下，然后添加现有项把他加进来。至此头文件都找到了，如图

![](https://upload-images.jianshu.io/upload_images/2587882-0c7231d8412fac1f.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/700)


找到bsdiff.cpp的文件，由于里面有main函数，我们不需要，把他改名成bsdiff_main，然后在末尾添加JNIEXPORT jint JNICALL Java_app_1update_1service_ServiceDiff_diff(JNIEnv *env, jclass cls, jstring oldFile_jstr, jstring newFile_jstr, jstring patchFile_jstr)函数来实现，具体实现看代码。只是调用拆分的方法。

## 4.eclpise调用dll动态库

把生成的dll动态库复制到src文件夹下，然后配置一下dll的搜索路径。

```
右击项目，从弹出的右键菜单中选择“Properties”，或者按Alt+Enter键。

弹出properties设置窗口，从左侧列表中找到“Java Build Path”，然后选择右侧的“libraries”选项卡，点击“JRE System Library”。

选择“Native library location”，在没有设置的情况下可以看见后面写的是“（None）”，点击“Edit”按钮。

弹设置对话框，把dll文件夹所在的目录复制粘贴到location path框中，点击OK按钮即可。返回properties窗口，点击OK按钮。

这样用System.loadLibrary("bsdiff");就可以调用动态库了。
```

## 5.测试拆分是否成功

网上下载了微信的6.0版本和6.6版本的apk包，进行拆分，拆分后生成weixin.patch，拆分是异步的，比较慢，等待一段时间，发现生成了weixin.patch，拆分成功。如图，大概节省了7M的流量

![](https://upload-images.jianshu.io/upload_images/2587882-24753c3995d14c26.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/631)
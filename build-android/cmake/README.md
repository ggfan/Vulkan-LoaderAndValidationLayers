Build Validation Layers with Android CMake
=========================================
Gradle's CMake library project in this directory builds layers into AAR; the project could be directly
added into application's gradle projects:
- settings.gradle:  add <include "path/to/the/this/lib/:layerLib"
- app's build.gradle:
```java
dependencies {
    // force debug layer lib for packing
    compile project(path: ':layerlib', configuration: 'debug')
}
```


# Glare Core


A bunch of C++ code used in various Glare Technologies Limited products - [Indigo Renderer](https://www.indigorenderer.com/), [Chaotica](https://www.chaoticafractals.com/), 
[Substrata](https://substrata.info/) and other projects.

Most of this code was written by Nicholas Chapman (Ono-Sendai).  Some was also written by Thomas Ludwig (lycium) and Yves Collé (fused).

There's also a bunch of third-party libraries copied into this repo for building convenience.

Our code:

## graphics

Various computer graphics utility classes, image/texture loading etc.

## maths

Various maths utility classes.  SSE vector maths, matrix maths etc.

## networking

TCP and UDP socket classes, TLS sockets, an HTTP client, a WebSocket class etc.

## opencl

Wrappers for OpenCL, the best GPU compute API.

## opengl

A reasonably lightweight but prettty fast OpenGL physically based rendering engine. 
This is the OpenGL preview for Indigo Renderer, and the rendering engine for Substrata.

## physics

Some ray-mesh acceleration structures etc.

## simpleraytracer

Some ray tracing geometry classes etc.

## video

Code for writing out high-definition video files on Windows and Mac, using OS APIs.

## webserver

Code for running a HTTP server, using the networking classes in networking.  Supports HTTP 1.1 and TLS.

## utils

Lots of different utility classes.  Some highlights:

### BestFitAllocator 

Best-fit memory allocator.  Useful for packing stuff into larger buffers, like vertex buffers.

### CircularBuffer

Circular (ring) buffer.  Useful for queues while avoiding memory allocations.


### Database

In-memory databased backed by disk.  Pretty sweet.  No doubt faster than your database.


### HashMap

Fast hash table/map.  Much faster than std::unordered_map.


### HashSet

Fast hash table/set.  Much faster than std::unordered_set.


### ManagerWithCache

Something like a std::unordered_map, but also each inserted item can be 'used' or 'unused'.
Unused items are stored in a list that is sorted by the order in which they became unused,
so it can be used as a LRU cache.
The least recently used item can be removed from the cache with removeLRUUnusedItem().

### MemMappedFile

Provides read-only access to a file through memory mapping.   The fastest way (AFAIK) of reading files.

### Parser

Parsing class.  I use it everywhere.  Don't use regular expressons, parse stuff properly :)


### Sort.h

Some really fast parallel radix sorting code.


### StringUtils

Lots of really useful stuff in here.  Used almost everywhere in my code.


### TaskManager

Nice and simple tasking system for multithreaded code.

### ThreadSafeQueue

The backbone of a lot of multithreaded code.

### TopologicalSort

Uses something like Kahn's algorithm, see See https://en.wikipedia.org/wiki/Topological_sorting
Instead of operating on a graph however, we will use maps to sets.

### Vector

Custom version of std::vector with slightly different (faster) semantics and some extra methods.


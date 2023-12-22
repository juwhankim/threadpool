# threadpool

## Outline
This is yet another implementation of work-stealing threadpool in C++. Internal working follows the common logic. However the implementation tries to

- Properly align array structures.
- Pad the classes' internal variables to avoid false sharing in cache.
- Avoid unneccessary locks as much as possible(For enabling proper queue sleep wake cycle, locks are used at minimal level for the case of normal threadpool operation.).
- Cleanly separate implementations of the normal threadpool and the greedy threadpool, where in the case of the greedy one, work stealing queues keep polling the master queue for new jobs. Also, the greedy pool can be started and stopped explicitly, so that it can be used in computationally heavy sections where start and end positions are clear.
- Enable C++ style usage, where functors, class methods, and normal functions can be thrown into the pool without specifiying the function signatures.
- Heavy usage of C++11 meta programming features to make heavy lifting of system parameter calculation at compile time. For example, endian detection, word byte length, function signature calculation, and etc.
- Easy joining via receipts. Thread branching thread simply wait for the receipt issued by the branched work thread.
- Minimal(or NO) dependency on pre-defined macros, defined by different compilers, resulting in cleaner and more readable code.

## What's included
* A work stealing threadpool: fully automatic execution. It goes into sleep when no work is fed, and wakes up on demand.
* A greedy work stealing threadpool: semi-automatic execution. When kick started it keeps polling for work until it is explicitly stopped.
* A logger: A template based logger class. The class is built at compile time, including the output format and level. By default, it is suppressed in the threadpool implementation. However the utility class is included here to help understanding the logging messages included in the source code of threadpool.

## Oddity
* Instead of using the standard C++11 template library, most features are implemented independently in juwhan_std.h. In the course of development, it is noted that the level of completion for the standard template library varies among OSs(especially in mobile platform), and was decided to provide an independent one.
* Namespace **juwhan**. Simply, I could not come up with a better namespace and juwhan is my name. Feel free to search and destroy the namespace and change it to your name or something more suitable.
* Reimplementation of thread and threadlocal classes. Again, availability of the classes in the standard template library varies. Hence, reimplemented using pthread(which is almost universally available).

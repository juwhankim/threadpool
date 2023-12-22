#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <chrono>
#include <time.h>

#include "greedy_threadpool.h"

using namespace juwhan;
using namespace std;
using namespace std::chrono;

#define N 600000
#define M 1
#define PLIM 10

class RandomSequenceOfUnique {
private:
    unsigned int m_index;
    unsigned int m_intermediateOffset;

    static unsigned int permuteQPR(unsigned int x) {
        static const unsigned int prime = 4294967291u;
        if (x >= prime)
            return x;  // The 5 integers out of range are mapped to themselves.
        unsigned int residue = ((unsigned long long) x * x) % prime;
        return (x <= prime / 2) ? residue : prime - residue;
    }

public:
    RandomSequenceOfUnique(unsigned int seedBase, unsigned int seedOffset) {
        m_index = permuteQPR(permuteQPR(seedBase) + 0x682f0161);
        m_intermediateOffset = permuteQPR(permuteQPR(seedOffset) + 0x46790905);
    }

    unsigned int next() {
        return permuteQPR((permuteQPR(m_index++) + m_intermediateOffset) ^ 0x5bf03635);
    }
};


#define pivot_index() (begin+(end-begin)/2)
#define swap(a, b, t) ((t)=(a),(a)=(b),(b)=(t))

void serial_qsort(unsigned int array[], unsigned int begin, unsigned int end) {
    /*** Use of static here will reduce memory footprint, but will make it thread-unsafe ***/
    unsigned int pivot;
    unsigned int t;     /* temporary variable for swap */
    if (end > begin) {
        unsigned int l = begin + 1;
        unsigned int r = end;
        swap(array[begin], array[pivot_index()], t); /*** choose arbitrary pivot ***/
        pivot = array[begin];
        while (l < r) {
            if (array[l] <= pivot) {
                l++;
            } else {
                while (l < --r && array[r] >= pivot) /*** skip superfluous swaps ***/
                    ;
                swap(array[l], array[r], t);
            }
        }
        l--;
        swap(array[begin], array[l], t);
        serial_qsort(array, begin, l);
        serial_qsort(array, r, end);
    }
}

void parallel_qsort(unsigned int array[], unsigned int begin, unsigned int end) {
    /*** Use of static here will reduce memory footprint, but will make it thread-unsafe ***/
    unsigned int pivot;
    unsigned int t;     /* temporary variable for swap */
    if (end > begin) {
        unsigned int l = begin + 1;
        unsigned int r = end;
        swap(array[begin], array[pivot_index()], t); /*** choose arbitrary pivot ***/
        pivot = array[begin];
        while (l < r) {
            if (array[l] <= pivot) {
                l++;
            } else {
                while (l < --r && array[r] >= pivot) /*** skip superfluous swaps ***/
                    ;
                swap(array[l], array[r], t);
            }
        }
        l--;
        swap(array[begin], array[l], t);
        greedy_threadpool_receit<void> r1, r2;
        if ((l - begin) > PLIM) {
            r1 = gtp.submit(parallel_qsort, array, begin, l);
        } else {
            serial_qsort(array, begin, l);
        }
        if ((end - r) > PLIM) {
            r2 = gtp.submit(parallel_qsort, array, r, end);
        } else {
            serial_qsort(array, r, end);
        }
        r1.wait();
        r2.wait();
    }
}

#undef swap
#undef pivot_index


int main() {

    // Generate a non-repeating random array.
    unsigned int arr0[N];
    unsigned int arr[N];
    unsigned int arr_p[N];
    unsigned int seed{static_cast<unsigned int>(time(NULL))};
    RandomSequenceOfUnique rsu(seed, seed + 1);
    for (auto i = 0; i < N; ++i) arr0[i] = rsu.next();

    // Serial case.
    //
    // Measure time now.
    steady_clock::time_point tim = steady_clock::now();
    for (auto i = 0; i < M; ++i) {
        for (auto j = 0; j < N; ++j) arr[j] = arr0[j];
        serial_qsort(arr, 0, N - 1);
    }
    // Measure duration.
    auto dur = steady_clock::now() - tim;
    // Print out.
    cout << "Linear sorting of length " + ::juwhan::to_string(N) + ", " + ::juwhan::to_string(M) + " times took "
         << duration_cast<milliseconds>(dur).count() << " milliseconds." << endl;
    // cout << "The result is:";
    // for(auto i=0; i<N; ++i) cout << " " << arr[i];
    // cout << endl;


    // Parallel case.
    //
    // Measure time now.
    gtp.go();
    tim = steady_clock::now();
    for (auto i = 0; i < M; ++i) {
        for (auto j = 0; j < N; ++j) arr_p[j] = arr0[j];
        parallel_qsort(arr_p, 0, N - 1);
    }
    // Measure duration.
    dur = steady_clock::now() - tim;
    // Print out.
    cout << "parallel sorting of length " + ::juwhan::to_string(N) + ", " + ::juwhan::to_string(M) + " times took "
         << duration_cast<milliseconds>(dur).count() << " milliseconds." << endl;
    // cout << "The result is:";
    // for(auto i=0; i<N; ++i) cout << " " << arr[i];
    // cout << endl;

    gtp.destroy();

    // Verify.
    for (auto i = 0; i < N; ++i) if (arr[i] != arr_p[i]) throw "Something's wrong";
    cout << "The result verification passes OK." << endl;

    return 0;
}










# CS537 P3A: Parallel Zip
A simple compression tool that makes use of multi-threading. It uses <i>run-length encoding (RLE)</i> to compress the input of one or many files.

##### Table of Contents
* [Installation](#installation)
* [Implementation](#implementation)
  * [Producer](#producer)
  * [Consumer](#consumer)
  * [Print](#print)
* [Performance](#performance)
* [Acknowledgement](#acknowledgement)

## Installation
To compile the program, run:
<pre><code>gcc -o pzip -Wall -Werror -pthread -O pzip.c</code></pre>
To run the program, execute:
<pre><code>./pzip [FILE_1] [FILE_2] ...</code></pre>

## Implementation
All producer and consumers will read and parse the data to an 2-dimensional array called <code>results</code>, where the first index indicates the file, and the second index indicates the parsed data.

### Producer
There is one producer in this implementation. The producer reads each file to memory by using <code>mmap</code>, split the file into pages, and push a <code>buffer</code> containing the data for the consumer to consume. When done, the producer will send a terminating buffer to the client after waking them up.

### Consumer
There are <code>n</code> consumers depending on the number of CPU cores available. The consumer waits for the <code>buffer</code> from the producer, consume the data by parsing each count and character, and stores it at each <code>results</code>.

### Print
The program prints the <code>results</code> after waiting for all consumers to finish their works.

## Performance
The implementation was tested in <code>royal-14</code> by running <code>time ./test-pzip.sh</code> inside the <code>/nobackup</code> folder with the following result:
<pre><code>test 1: passed
test 2: passed
test 3: passed
test 4: passed
test 5: passed
test 6: passed
test 7: passed
test 8: passed
test 9: passed
test 10: passed
test 11: passed
test 12: passed
test 13: passed
test 14: passed
test 15: passed
test 16: passed
test 17: passed

real	0m25.424s
user	0m22.641s
sys	0m3.315s</pre></code>

## Acknowledgement
This is an assignment for a class [Comp Sci. 537: Introduction to Operating Systems](https://pages.cs.wisc.edu/~remzi/Classes/537/Fall2021/) by [Remzi Arpaci-Dusseau](https://pages.cs.wisc.edu/~remzi/). Please refer to [this repo](https://github.com/remzi-arpacidusseau/ostep-projects/tree/master/concurrency-pzip) for more details about the assignment.

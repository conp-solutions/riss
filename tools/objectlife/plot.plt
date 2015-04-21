set terminal postscript eps color enhanced
set key right bottom
set linestyle 1 lw 3 lt rgb "#000000"
set key box linestyle 1

set title "object life time frequencies"
set xlabel "lifetime in us"
set ylabel "frequency"
set output "lifetime.eps"
set logscale y 2
plot "lifetime.dat" using 1:2 with points

set title "lifetime of object with a given size"
set xlabel "lifetime in us"
set ylabel "size"
#set log scales for this experiment
set logscale x 10
set logscale y 2
set output "lifetime_size.eps"
plot "lifetime_size.dat" using 1:2 with points

# let gnuplot determine range
set autoscale
unset logscale
set title "alloced memory per time"
set xlabel "time in us"
set ylabel "alloced memory"
set output "alloc_time.eps"
plot "memory_time.dat" using 1:2 with lines

set title "freed memory per time"
set xlabel "time in us"
set ylabel "alloced memory"
set output "free_time.eps"
plot "memory_time.dat" using 1:3 with lines

set title "currend memory per time"
set xlabel "time in us"
set ylabel "currend memory"
set output "memory_time.eps"
plot "memory_time.dat" using 1:4 with lines

# alloc and free in on diagram
set title " alloc and free memory per time"
set xlabel "time in us"
set ylabel "memory"
set output "movement_time.eps"
plot "memory_time.dat" using 1:2 with lines lt rgb "#FF0000" title "alloc", "memory_time.dat" using 1:3 with lines  lt rgb "#000080" title "free"

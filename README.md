# ws5-rtos-skeleton

* Name: Xinyi Gong
* GitHub username: xinyigbytes10
* GitHub Repository URL: 
* Description of hardware: (embedded hardware, laptop, etc) 





## R1

Yes, the task set is RM schedulable. No adjustment to Task B is needed.

Under Rate Monotonic Scheduling (RMS), tasks with shorter periods have higher priorities.

| Task | CPU Time (ms) | Period (ms) | Priority |
|------|---------------|-------------|----------|
| A | 150 | 750 | 3 (Highest) |
| B | 300 | 1200 | 2 |
| C | 450 | 1500 | 1 (Lowest) |


U = 150/750 + 300/1200 + 450/1500

U = 0.20 + 0.25 + 0.30 = 0.75 = 75%

For 3 tasks, the RMS utilization bound is:

U_max = 3(2^(1/3) - 1) = 0.7798 = 77.98%

Since 75% < 77.98%, the task set is RM schedulable.

### Scheduling Diagram

![](images\R1.png)



All tasks meet their deadlines. The original task set is schedulable under RMS, so Task B's CPU time does not need to be changed.


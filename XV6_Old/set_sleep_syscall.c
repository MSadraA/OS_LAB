#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "date.h"
#define WRONGINPUTTRYAGAIN "invalid command!try again\n"
#define ISSUEOCCURED "issue occured\n"
#define STARTEDYEARBYCMOSTIME 2000
int is_leap_year(int year)
{
    return (year%4==0 && (year%100!=0 || year%400==0));
}
int convert_time_to_second(struct rtcdate* recorded_time)
{
    int days_per_month[]={0,31,28,31,30,31,30,31,31,30,31,30,31},total_second=0;
    int day_to_second=0,year_by_cmostime=recorded_time->year,second_by_cmostime=recorded_time->second,hour_by_cmostime=recorded_time->hour,minute_by_cmostime=recorded_time->minute;
    for (int years=STARTEDYEARBYCMOSTIME;years<year_by_cmostime;years++) 
    {
      if(is_leap_year(years))
        day_to_second+=366;
      else
        day_to_second+=365;
    }
    for (int i=1;i<recorded_time->month;i++) {
      day_to_second+=days_per_month[i];
      if(i==2 && is_leap_year(year_by_cmostime))
        day_to_second+=1;
    }
    day_to_second+=recorded_time->day-1;
    total_second=day_to_second*24*3600+hour_by_cmostime*3600 +minute_by_cmostime*60 +second_by_cmostime;
    return total_second;
}
int main(int argc,char *argv[])
{
    int diffrence=0,before_sleep_in_second=0,after_sleep_in_second=0;
    struct rtcdate before_sleep,after_sleep;
    if(argc!=2)
    {
        printf(1,"%s\n",WRONGINPUTTRYAGAIN);
        exit();
    }
    int input_tick=atoi(argv[1]);
    get_system_time(&before_sleep);
    if(set_sleep_syscall(input_tick)<0)
    {
        printf(1,ISSUEOCCURED);
        exit();
    }
    get_system_time(&after_sleep);
    before_sleep_in_second=convert_time_to_second(&before_sleep);
    after_sleep_in_second=convert_time_to_second(&after_sleep);
    diffrence=after_sleep_in_second-before_sleep_in_second;
    printf(1,"result:%d\n",diffrence);
    exit();
}
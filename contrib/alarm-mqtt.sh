#!/bin/sh
#
# alarm-mqtt.sh  -- sample trigger script for writing events to mqtt for nxgipd daemon
# requires mosquitto_pub to be installed
#
# This gets called in response to events in monitored alarm system.
# Information about the even is passed in environment variables to
# this script.
#
# Common environment variables (all events):
#
# ALARM_EVENT_TYPE               This will be set to one the following
#                                values: "zone", "partition", or "log" 
#                                
# ALARM_EVENT_STATUS             This contains string describing the event
#
#
# Event type specific variables:
#
# Zone:
# ALARM_EVENT_ZONE               Zone Number
# ALARM_EVENT_ZONE_NAME          Zone Name
# ALARM_EVENT_ZONE_FAULT         Zone Fault status: 1=Fault, 0=OK 
# ALARM_EVENT_ZONE_TROUBLE       Zone Trouble status: 1=Trouble, 0=No Trouble
# ALARM_EVENT_ZONE_TAMPER        Zone Tamper status: 1=Tamper, 0=No Tamper
# ALARM_EVENT_ZONE_BYPASS        Zone Bypass status: 1=Bypassed, 0=Not Bypassed
# ALARM_EVENT_ZONE_ARMED         Zone Armed: 1=Armed, 0=Not Armed
# 
# Partition:
# ALARM_EVENT_PARTITION          Partition Number
# ALARM_EVENT_PARTITION_ARMED    Partition Armed status: 1=Armed, 0=Not Armed
# ALARM_EVENT_PARTITION_READY    Partition Ready: 1=Ready, 0=Not Ready
# ALARM_EVENT_PARTITION_STAY     Partition Stay Mode: 1=On, 0=Off
# ALARM_EVENT_PARTITION_CHIME    Partition Chime Mode: 1=On, 0=Off
# ALARM_EVENT_PARTITION_ENTRY    Partition Entry Delay: 1=Start, 0=End
# ALARM_EVENT_PARTITION_EXIT     Partition Exit Delay: 1=Start, 0=End
# ALARM_EVENT_PARTITION_ALARM    Partition Alarm status: 1=On, 2=Off
#
# Log:
# ALARM_EVENT_LOG_TYPE           Raw log entry type (see nx-584.c)
# ALARM_EVENT_LOG_NUM            Log entry number
# ALARM_EVENT_LOG_LOGSIZE        Max log number
# (following are only present if log entry defines them)
# ALARM_EVENT_LOG_PARTITION      Partition this entry refers to (if defined)
# ALARM_EVENT_LOG_ZONE           Zone this entry refers to (if defined)
# ALARM_EVENT_LOG_USER           User this entry refers to (if defined)
# ALARM_EVENT_LOG_DEVICE         Device this entry refers to (if defined)
# (following define the time event was recorded according alarm panel clock)
# ALARM_EVENT_LOG_MONTH          Month (1..31)
# ALARM_EVENT_LOG_DAY            Day (of Month) (1..12)
# ALARM_EVENT_LOG_HOUR           Hour (0..23)
# ALARM_EVENT_LOG_MIN            Minutes (0..59)
# 


USER="userid"
PASS="password"

case "${ALARM_EVENT_TYPE}" in

    zone)
	mosquitto_pub -t nx584/zone$ALARM_EVENT_ZONE -m "$ALARM_EVENT_STATUS" -u $USER -P $PASS
	;;

    partition)
	mosquitto_pub -t nx584/partition$ALARM_EVENT_PARTITION -m "$ALARM_EVENT_STATUS" -u $USER -P $PASS
	;;

    log)
	mosquitto_pub -t nx584/log$ALARM_EVENT_LOG_NUM/${ALARM_EVENT_LOG_LOGSIZE} -m "$ALARM_EVENT_STATUS" -u $USER -P $PASS
	;;

    *)
	mosquitto_pub -t nx584/unknownevent -m "$ALARM_EVENT_STATUS" -u $USER -P $PASS
	exit 1
	;;
esac


# eof :-)


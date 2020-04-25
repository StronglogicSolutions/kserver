#!/usr/bin/env bash

## Input param should be a Schedule ID
ID=$1

## DB Select helper
function kiq_select() {
  echo $(psql kiq -Ukiqadmin --pset="pager=off" --pset="footer=off" -t -c "SELECT $1 from schedule where id = $2;" | sed 's/[ \t]*$//')
}

## Set environment variables for execution
function set_env_vars() {
  # Fetch values from database
  ENVFILE=$(kiq_select envfile $ID)
  FLAGS=$(kiq_select flags $ID)
  MASK=$(kiq_select mask $ID)
  FILENAMES=$(psql kiq -Ukiqadmin --pset="pager=off" --pset="footer=off" -t -c "SELECT name FROM file WHERE sid = $ID;" | sed 's/[ \t]*$//')
#  FILENAMES=$(psql kiq -Ukiqadmin --pset="pager=off" --pset="footer=off" -t -c "SELECT '--filename=', name FROM file WHERE sid = $ID;" | sed -e 's/[ \t]*$//' -e 's/ | //')

  APP_PATH=$(psql kiq -Ukiqadmin --pset="pager=off" --pset="footer=off" -t -c "SELECT path FROM apps WHERE mask = $MASK;" | sed 's/[ \t]*$//')
  MEDIA_FILE=$(echo $FILENAME | sed 's/[ \t]*$//')

  # source environment variable
  source /data/c/kserver/$(echo $ENVFILE | sed 's/[ \t]*$//')
}

## Build execute command and execute!
function execute() {
  # Build execution string
  PARAMS="--description=\"'$DESCRIPTION'\" --header=\"'$HEADER'\" --promote_share=\"'$PROMOTE_SHARE'\" --requested_by=\"'$REQUESTED_BY'\" --requested_by_phrase=\"'$REQUESTED_BY_PHRASE'\" --hashtags=\"'$HASHTAGS'\" --link_bio=\"'$LINK_BIO'\" --user=\"'$USER'\" --media=\"'$FILE_TYPE'\""
  # Append filename params
  for name in $FILENAMES
    do
      PARAMS=$(echo $"$PARAMS")" "--filename="'$name'"
  done
# Execute
  EXECUTE_STRING=$APP_PATH" "$PARAMS
  eval $EXECUTE_STRING
}

### Main script ###
set_env_vars
echo $(execute)

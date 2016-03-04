<?php

/**
 * @file MeetingCloneJournal.inc
 * 
 *  Represent the attending data structure. Pass this to drupal_write_record to record.
 */
class MeetingCloneJournal {

  var $repid = '';
  var $nid = '';
  var $created = '';
  var $attending = FALSE;

  /**
   * Construct with all variables set.
   */
  public function __construct($repid, $nid) {
    $this->repid = $repid;
    $this->$nid = $nid;
    $this->created = time();
  }

}

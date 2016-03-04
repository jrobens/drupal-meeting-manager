<?php

/*
 * @doc
 * 
 * Encapsulate the replicants within a replication.
 * 
 * jrobens@interlated.com.au 201301
 * 
 * 
 */

/**
 * Encapsulate a list of replications. That is an index of the replications that have been run.
 */
class MeetingCloneReplication {

  protected $replication_id = "";
  protected $replicated_nodes = array();

  public function __construct($replication_id) {
    $this->replication_id = $replication_id;
    $this->find();
  }

  /**
   * 
   * @return array
   *   the list of replications. 
   */
  public function get() {
    return $this->replicated_nodes;
  }

  /**
   * Delete all the nodes related to a replication.
   */
  public function delete() {
    // Put in checks for errors.
    foreach ($this->replicated_nodes as $nid) {
      node_delete($nid);
    }

    // Delete the replication as well. 
    db_delete('meetingclone_history')
        ->condition('repid', $this->replication_id, '=')
        ->execute();
  }

  /**
   * Find nodes in a replication.
   * 
   * @return  
   */
  protected function find() {
    // Try/catch? error checking.

    $query = db_select('meetingclone_history', 'mh');
    $nids = $query
        ->fields('mh', array('repid', 'nid', 'created'))
        ->havingCondition("repid", $this->replication_id, '=')
        ->execute()
        ->fetchAllAssoc('nid');

    foreach ($nids as $found_nid) {
      if (!empty($found_nid->nid)) {
        $this->replicated_nodes[] = $found_nid->nid;
      }
      else {
        watchdog(MEETINGCLONE_MODULE_NAME, "Found a replication entry without a node");
      }
    }

    return TRUE;
  }

}
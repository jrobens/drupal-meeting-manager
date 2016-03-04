<?php

/*
 * @doc
 * Display the replicants for a particular replication. 
 * 
 * jrobens@interlated.com.au 201301
 * 
 */

/**
 * List all the replications found on the system. 
 * 
 * Just a list. The deletion can be run from the replication table.
 */
function meetingclone_list_replication_form($form, &$form_state, $replication_id) {
  $replication = new MeetingCloneReplication($replication_id);

  $form['replication'] = array(
    '#type' => 'fieldset',
    '#title' => t('Nodes for replication.'),
  );

  $form['replication'] = array(
    '#type' => 'fieldset',
    '#title' => t('Replication ') . check_plain($replication_id),
  );

  // Array of name value pairs.
  $replication_data = $replication->get($replication_id);

  // Theme this.
  $replication_text = "<ul>";
  $nodes_loaded = entity_load('node', $replication_data);
  foreach ($nodes_loaded as $node) {
    $replication_text .= "<li>" . check_plain($node->title) . ' (' . $node->nid . ')' . '</li>';
  }
  $replication_text .= '</ul>';

  // Just make it a static list. Perhaps allow a single node to be deleted. Probably should not duplicate the standard content
  // editing facilities.
  $form['replication']['get'] = array(
    '#markup' => $replication_text
  );

  drupal_set_title('Nodes for replication ');

  return $form;
}


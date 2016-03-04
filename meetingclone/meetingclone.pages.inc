<?php

// $Id: meetingclone.pages.inc,v 1.5 2011/02/01 01:22:56 pwolanin Exp $

/**
 * Menu callback to configure module settings.
 */
function meetingclone_settings($form, &$form_state) {

  $field_list = meetingclone_field_list();
  $populate_meetings = meetingclone_meeting_nodes();

  $form['basic'] = array(
    '#type' => 'fieldset',
    '#title' => t('General settings'),
  );

  $form['basic']['meetingclone_date_field'] = array(
    '#type' => 'select',
    '#options' => $field_list,
    '#title' => t('Template meeting date field'),
    '#default_value' => variable_get('meetingclone_date_field', ''),
    '#description' => t('Field containing the date of the meeting so that we can set it for each meeting copy.'),
  );

  $form['basic']['meetingclone_submission_cutoff_field'] = array(
    '#type' => 'select',
    '#options' => $field_list,
    '#title' => t('Template meeting cutoff date field'),
    '#default_value' => variable_get('meetingclone_submission_cutoff_field', ''),
    '#description' => t('Members can only submit agenda items up to the date in this field.'),
  );
  
  $form['basic']['meetingclone_clone_template'] = array(
    '#type' => 'select',
    '#title' => 'Select a Meeting',
    '#options' => $populate_meetings,
    '#description' => t('Select source meeting to use for cloning.'),
    '#default_value' => variable_get('meetingclone_clone_template', ''),
    "#empty_option" => t('- Select -'),
  );

  return system_settings_form($form);
}

function meetingclone_meeting_nodes() {
  $query_etids = array();

  $node_types[] = array('meeting');
  
  // Add a query tag to allow us to alter the query.
  //$query_key = 'associatenode_' . implode('', $node_types);
  
  $query = db_select('node', 'n');
  $query->fields('n', array('nid', 'title'))
      ->condition('n.type', $node_types, 'IN')
      ->orderBy('title', 'ASC');
  
  if ($result = $query->execute()->fetchAllKeyed()) {
    $query_etids = $result;
  }
  return $query_etids;
}

/**
 *  Menu callback: prompt the user to confirm the operation
 */
function meetingclone_node_check($template_node) {
  $method = variable_get('meetingclone_method', 'prepopulate');
  switch ($method) {
    // If reviewing a populated node, then either display the node or save it.
    case 'save-edit':
      if (variable_get('meetingclone_nodes_without_confirm', FALSE)) {
        // Hooks in when we call save.
        $new_nid = clone_node_save($template_node->nid);
        drupal_goto('node/' . $new_nid . '/edit');
      }
      break;
    case 'prepopulate':
    default:
      $cutoff_field = variable_get('meetingclone_submission_cutoff_field', '');
      if (empty($cutoff_field)) {
        drupal_set_message(t("Submission cut off field is not set. We won't be able to set this in the clone. Please configure the module first. ") . l('/admin/config/meetingclone/settings', 'admin/config/meetingclone/settings'), 'warning', FALSE);
      }

      // Otherwise prepulate the node
      return meetingclone_node_prepopulate($template_node);
      break;
  }
}

/**
 * Clones a node - prepopulate a node editing form
 * 
 * This gets called. But so does _node_save.
 * 
 * The node stays in any existing group. The clone function just copies group_audience.
 * 
 *  @param
 *    original_node - loaded node of the node to clone
 *    group_node - loaded node of the group to link created nodes to.
 */
function meetingclone_node_prepopulate($original_node) {
  if (isset($original_node->nid)) {
    global $user;

    if (meetingclone_is_permitted($original_node->type)) {
      $node = clone $original_node;

      $node->nid = NULL;
      $node->vid = NULL;
      $node->tnid = NULL;
      $node->name = $user->name;
      $node->uid = $user->uid;
      $node->created = NULL;
      //  $node->menu = meetingclone_node_clone_menu_link($original_node);
      if (isset($node->book['mlid'])) {
        $node->book['mlid'] = NULL;
      }
      $node->path = NULL;
      $node->files = array();
      $node->title = t('Clone of !title', array('!title' => $node->title));

      // Mark as temporary - to be deleted
      $node->meetingclone_temponly = TRUE;

      // Add an extra property as a flag.
      if (!isset($node->meetingclone_serial)) {
        $node->meetingclone_serial = $original_node->nid . '_' . time();
        $node->meetingclone_original = $original_node->nid;
      }

      drupal_set_title(check_plain($node->title));

      if (variable_get('meetingclone_reset_' . $node->type, FALSE)) {
        $node_options = variable_get('node_options_' . $node->type, array('status', 'promote'));
        // Fill in the default values.
        foreach (array('status', 'moderate', 'promote', 'sticky', 'revision') as $key) {
          $node->$key = in_array($key, $node_options);
        }
      }

      // Let other modules do special fixing up.
      // The function signature is: hook_meetingclone_node_alter(&$node, $context)
      // $context is an array with two elements, 'method' and 'original_node',
      // where 'method' is either 'prepopulate' or 'save-edit'.
      $context = array('method' => 'prepopulate', 'original_node' => $original_node);
      drupal_alter('meetingclone_node', $node, $context);

      // Make sure the file defining the node form is loaded.
      module_load_include('inc', 'node', 'node.pages');
      return drupal_get_form($node->type . '_node_form', $node);
    }
  }
}

/**
 * Implements hook_node_insert().
 * @param type $node
 */
function meetingclone_node_insert($node) {
  if (isset($node->meetingclone_temponly) && $node->meetingclone_temponly == TRUE) {
    node_delete($node->nid);
    if (module_exists('og') && !empty($node->group_audience[LANGUAGE_NONE][0]['gid'])) {
      $group = og_load($node->group_audience[LANGUAGE_NONE][0]['gid']);
      drupal_goto(url('node/' . $group->etid));
    }
  }
}

/**
 * Helper function to populate the first dropdown. This would normally be
 * pulling data from the database.
 *
 * @return array of options
 */
function meetingclone_field_list() {
  $fields = array();
  $fields[0] = 'Select...';
  $result = db_query("SELECT field_config_instance.field_name AS field_name FROM {field_config_instance} field_config_instance WHERE bundle=:type", array(':type' => 'meeting'));

  foreach ($result as $record) {
    $fields[$record->field_name] = $record->field_name;
  }
  return $fields;
}

/**
 * Helper function to populate the first dropdown. This would normally be
 * pulling data from the database.
 *
 * @return array of options
 */
function meetingclone_node_list() {
  $fields = array();
  $fields[0] = 'Select...';
  $result = db_query("SELECT node.nid AS nid,
    node.title  AS title
    FROM {node} node WHERE node.type=:type", array(':type' => 'meeting'));

  foreach ($result as $record) {
    $fields[$record->nid] = $record->title;
  }
  return $fields;
}

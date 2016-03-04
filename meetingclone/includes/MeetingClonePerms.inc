<?php

/*
 * Interface to permissions. I want to use 'global php' in views and not have too much code in there. 
 * 
 * Create permissions methods herre. 
 * 
 * jrobens@interlated.com.au 201301
 */

class MeetingClonePerms {

  public static function accessCloning($nid) {
    module_load_include('module', 'meetingclone', 'meetingclone');
    if (!meetingclone_access_cloning(node_load($nid), 'clone')) {
      return FALSE;
    }
    return TRUE;
  }

}

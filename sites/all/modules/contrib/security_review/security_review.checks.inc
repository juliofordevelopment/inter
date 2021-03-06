<?php

/**
 * @file
 * Checks for security_review module.
 *
 */

/**
 * Check that files aren't writeable by the server.
 */
function security_review_check_file_perms() {
  $result = TRUE;
  // Extract ending folder for file directory path.
  $file_path = rtrim(variable_get('file_public_path', conf_path() . '/files'), '/');
  $file_path = substr($file_path, strrpos($file_path, '/') + 1);
  // Set files to ignore.
  $ignore = array('..', 'CVS', $file_path);
  $files = _security_review_check_file_perms_scan('.', $ignore);

  // Try creating or appending files.
  // Assume it doesn't work.
  $create_status = $append_status = FALSE;

  $directory = drupal_get_path('module', 'security_review');
  // Write a file with the timestamp
  $file = './' . $directory . '/file_write_test.' . date('Ymdhis');
  if ($file_create = fopen($file, 'w')) {
    $create_status = fwrite($file_create, t("This is a vulnerable directory.\n"));
    fclose($file_create);
  }
  // Try to append to our IGNOREME file.
  $file = './'. $directory . '/IGNOREME.txt';
  if ($file_append = fopen($file, 'a')) {
    $append_status = fwrite($file_append, date('Ymdhis') . "\n");
    fclose($file_append);
  }

  if (count($files) || $create_status || $append_status) {
    $result = FALSE;
  }
  return array('result' => $result, 'value' => $files);
}

function _security_review_check_file_perms_scan($directory, $ignore) {
  $items = array();
  if ($handle = opendir($directory)) {
    while (($file = readdir($handle)) !== FALSE) {
      // Don't check hidden files or ones we said to ignore.
      if ($file[0] != "." && !in_array($file, $ignore)) {
        $file = $directory . "/" . $file;
        if (is_dir($file)) {
          $items = array_merge($items, _security_review_check_file_perms_scan($file, $ignore));
          if (is_writable($file)) {
            $items[] = preg_replace("/\/\//si", "/", $file);
          }
        }
        elseif (is_writable($file)) {
          $items[] = preg_replace("/\/\//si", "/", $file);
        }
      }
    }
    closedir($handle);
  }
  return $items;
}

function security_review_check_file_perms_help($result = NULL) {
  $element['title'] =  t("Web server file system permissions");
  $element['descriptions'][] = t("It is dangerous to allow the web server to write to files inside the document root of your server. Doing so would allow Drupal to write files that could then be executed. An attacker might use such a vulnerability to take control of your site. An exception is the files directory which Drupal needs permission to write to in order to provide features like file attachments.");
  $element['descriptions'][] = t("In addition to inspecting files, this test attempts to create and write to files. Look in your security_review module directory on the server for files named file_write_test.YYYYMMDDHHMMSS and for a file called IGNOREME.txt which gets a timestamp appended to it if it is writeable.</p>");
  $element['descriptions'][] = t("<a href='!link'>Read more about file system permissions in the handbooks.</a>", array('!link' => url('http://drupal.org/node/244924')));

  $last_check = security_review_get_last_check('security_review', 'file_perms');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    if (is_null($result)) {
      $result = security_review_check_file_perms();
    }
    $element['findings']['descriptions'][] = t('It is recommended that the following files or directories be corrected.');
    foreach ($result['value'] as $file) {
      $element['findings']['items'][] = array(
        'safe' => check_plain($file),
        'raw' => $file,
      );
    }
  }
  return $element;
}

/**
 * Check for formats that do not have HTML filter that can be used by untrusted users.
 */
function security_review_check_input_formats() {
  $result = TRUE;
  $formats = filter_formats();
  $check_result_value = array();
  // Check formats that are accessible by untrusted users.
  $untrusted_roles = security_review_untrusted_roles();
  $untrusted_roles = array_keys($untrusted_roles);
  foreach ($formats as $id => $format) {
    $format_roles = filter_get_roles_by_format($format);
    $intersect = array_intersect(array_keys($format_roles), $untrusted_roles);
    if (!empty($intersect)) {
      // Untrusted users can use this format.
      $filters = filter_list_format($format->format);
      // Check format for enabled HTML filter.
      if (in_array('filter_html', array_keys($filters)) && $filters['filter_html']->status) {
        $filter = $filters['filter_html'];
        // Check for unsafe tags in allowed tags.
        $allowed_tags = $filter->settings['allowed_html'];
        $unsafe_tags = security_review_unsafe_tags();
        foreach ($unsafe_tags as $tag) {
          if (strpos($allowed_tags, '<' . $tag . '>') !== FALSE) {
            // Found an unsafe tag
            $check_result_value['tags'][$id] = $tag;
          }
        }
      }
      elseif (!in_array('filter_html_escape', array_keys($filters)) || !$filters['filter_html_escape']->status) {
        // Format is usable by untrusted users but does not contain the HTML Filter or the HTML escape.
        $check_result_value['formats'][$id] = $format;
      }
    }
  }

  if (!empty($check_result_value)) {
    $result = FALSE;
  }
  return array('result' => $result, 'value' => $check_result_value);
}

function security_review_check_input_formats_help($result = NULL) {
  $element['title'] = t('Allowed HTML tags in text formats');
  $element['descriptions'][] = t("Certain HTML tags can allow an attacker to take control of your site. Drupal's input format system makes use of a set filters to run on incoming text. The 'HTML Filter' strips out harmful tags and Javascript events and should be used on all formats accessible by untrusted users.");
  $element['descriptions'][] = t("<a href='!link'>Read more about Drupal's input formats in the handbooks.</a>", array('!link' => url('http://drupal.org/node/224921')));
  $last_check = security_review_get_last_check('security_review', 'input_formats');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    if (is_null($result)) {
      $result = security_review_check_input_formats();
    }
    if (!empty($result['value']['tags'])) {
      $element['findings']['descriptions'][] = t('<a href="!link">Review your text formats.</a>', array('!link' => url('admin/config/content/formats')));
      $element['findings']['descriptions'][] = t('It is recommended you remove the following tags from roles accessible by untrusted users.');
      foreach ($result['value']['tags'] as $tag) {
        $element['findings']['items'][] = array(
          'safe' => $tag, // Tag doesn't need filtering cause it's not user-defined.
          'raw' => $tag,
        );
      }
    }
    elseif (!empty($result['value']['formats'])) {
      $element['findings']['descriptions'][] = t('The following formats are usable by untrusted roles and do not filter or escape allowed HTML tags.');
      foreach ($result['value']['formats'] as $id => $format) {
        $element['findings']['items'][] = array(
          'html' => l($format->name, 'admin/config/content/formats/' . $format->format),
          'safe' => check_plain($format->name),
          'raw' => $format->name,
        );
      }
    }
  }
  return $element;
}

function security_review_check_php_filter() {
  $result = TRUE;
  $formats = filter_formats();
  $check_result_value = array();
  // Check formats that are accessible by untrusted users.
  $untrusted_roles = security_review_untrusted_roles();
  $untrusted_roles = array_keys($untrusted_roles);
  foreach ($formats as $id => $format) {
    $format_roles = filter_get_roles_by_format($format);
    $intersect = array_intersect(array_keys($format_roles), $untrusted_roles);
    if (!empty($intersect)) {
      // Untrusted users can use this format.
      $filters = filter_list_format($format->format);
      // Check format for enabled PHP filter.
      if (in_array('php_code', array_keys($filters)) && $filters['php_code']->status) {
        $result = FALSE;
        $check_result_value['formats'][$id] = $format;
      }
    }
  }

  return array('result' => $result, 'value' => $check_result_value);
}

function security_review_check_php_filter_help($result = NULL) {
  $element['title'] = t('PHP Input Format');
  $element['descriptions'][] = t("Drupal's PHP Text Format allows for the interpretation and execution of PHP code via user-supplied input. Because this input runs in the context of Drupal itself it has access to everything Drupal does.");
  $last_check = security_review_get_last_check('security_review', 'untrusted_php');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    if (is_null($result)) {
      $result = security_review_check_php_filter();
    }
    if (!empty($result['value']['formats'])) {
      $element['findings']['descriptions'][] = t('The following formats are usable by untrusted roles and allow use of the PHP evaluator. You should edit the format to remove PHP use.');
      foreach ($result['value']['formats'] as $id => $format) {
        $element['findings']['items'][] = array(
          'html' => l($format->name, 'admin/config/content/formats/' . $format->format),
          'safe' => check_plain($format->name),
          'raw' => $format->name,
        );
      }
    }
  }
  return $element;
}

function security_review_check_error_reporting() {
  $error_level = variable_get('error_level', NULL);
  if (is_null($error_level) || intval($error_level) >= 1) {
    // When the variable isn't set, or its set to 1 or 2 errors are printed to the screen.
    $result = FALSE;
  }
  else {
    $result = TRUE;
  }
  return array('result' => $result);
}

function security_review_check_error_reporting_help($result = NULL) {
  $element['title'] = t('Error reporting');
  $element['descriptions'][] = t('As a form of hardening your site you should avoid information disclosure. Drupal by default prints errors to the screen and writes them to the log. Error messages disclose the full path to the file where the error occured.');
  if (is_null($result)) {
    $result = security_review_check_error_reporting();
  }
  if ($result['result'] === FALSE) {
    $element['findings']['descriptions'][] = t('You have error reporting set to both the screen and the log.');
    $element['findings']['descriptions'][] = t('<a href="!link">Alter error reporting settings.</a>', array('!link' => url('admin/config/development/logging')));
  }
  return $element;
}

/**
 * If private files is enabled check that the directory is not under the web root.
 *
 * There is ample room for the user to get around this check. @TODO get more sophisticated?
 */
function security_review_check_private_files() {
  // Get the default download method.
  $scheme = variable_get('file_default_scheme', '');
  // Most insecure configurations will be using the local private wrapper.
  if ($scheme == 'private') {
    $file_directory_path = variable_get('file_private_path', '');
    if (strpos($file_directory_path, '/') === 0) {
      // Path begins at root.
      $result = TRUE;
    }
    elseif (strpos($file_directory_path, '../') === 0) {
      // Path begins by moving up the system.
      $result = FALSE;
    }
    else {
      // Directory is relative (or crafty).
      $result = FALSE;
    }
  }
  else {
    return NULL;
  }
  return array('result' => $result);
}

function security_review_check_private_files_help($result = NULL) {
  $element['title'] = t('Private files');
  $element['descriptions'][] = t("If you have Drupal's private files feature enabled you should move the files directory outside of the web server's document root. While Drupal will control serving files when requested by way of content if a user knows the actual system path they can circumvent Drupal's private files feature. You can protect against this by specifying a files directory outside of the webserver root.");
  $last_check = security_review_get_last_check('security_review', 'private_files');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    $element['findings']['descriptions'][] = t('Your files directory is not outside of the server root.');
    $element['findings']['descriptions'][] = t('<a href="!link">Edit the files directory path.</a>', array('!link' => url('admin/config/media/file-system')));
  }
  return $element;
}

function security_review_check_query_errors($last_check = NULL) {
  $timestamp = NULL;
  $check_result_value = array();
  $query = db_select('watchdog', 'w')->fields('w', array('message', 'hostname'))
    ->condition('type', 'php')
    ->condition('severity', WATCHDOG_ERROR);
  if (!is_null($last_check)) {
    $query->condition('timestamp', $last_check['lastrun'], '>=');
  }
  $result = $query->execute();
  foreach ($result as $row) {
    if (strpos($row->message, 'SELECT') !== FALSE) {
      $entries[$row->hostname][] = $row;
    }
  }
  $result = TRUE;
  if (!empty($entries)) {
    foreach ($entries as $ip => $records) {
      if (count($records) > 10) {
        $check_result_value[] = $ip;
      }
    }
  }
  if (!empty($check_result_value)) {
    $result = FALSE;
  }
  else {
    // Rather than worrying the user about the idea of query errors we skip reporting a pass.
    return NULL;
  }
  return array('result' => $result, 'value' => $check_result_value);
}

function security_review_check_query_errors_help($result = NULL) {
  $element['title'] = t('Abundant query errors from the same IP');
  $element['descriptions'][] = t("Database errors triggered from the same IP may be an artifact of a malicious user attempting to probe the system for weaknesses like SQL injection or information disclosure.");
  $last_check = security_review_get_last_check('security_review', 'query_errors');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    $element['findings']['descriptions'][] = t('The following IPs were observed with an abundance of query errors.');
    if (is_null($result)) {
      $result = security_review_check_query_errors();
    }
    foreach ($result['value'] as $ip) {
      $element['findings']['items'][] = array(
        'safe' => check_plain($ip),
        'raw' => $ip,
      );
    }
  }
  return $element;
}

function security_review_check_failed_logins($last_check = NULL) {
  $result = TRUE;
  $timestamp = NULL;
  $check_result_value = array();
  $query = db_select('watchdog', 'w')->fields('w', array('message', 'hostname'))
    ->condition('type', 'php')
    ->condition('severity', WATCHDOG_NOTICE);
  if (!is_null($last_check)) {
    $query->condition('timestamp', $last_check['lastrun'], '>=');
  }
  $result = $query->execute();
  foreach ($result as $row) {
    if (strpos($row->message, 'Login attempt failed') !== FALSE) {
      $entries[$row->hostname][] = $row;
    }
  }
  if (!empty($entries)) {
    foreach ($entries as $ip => $records) {
      if (count($records) > 10) {
        $check_result_value[] = $ip;
      }
    }
  }
  if (!empty($check_result_value)) {
    $result = FALSE;
  }
  else {
    // Rather than worrying the user about the idea of failed logins we skip reporting a pass.
    return NULL;
  }
  return array('result' => $result, 'value' => $check_result_value);
}

function security_review_check_failed_logins_help($results = NULL) {
  $element['title'] = t('Abundant failed logins from the same IP');
  $element['descriptions'][] = t("Failed login attempts from the same IP may be an artifact of a malicous user attempting to brute-force their way onto your site as an authenticated user to carry out nefarious deeds. ");

  $last_check = security_review_get_last_check('security_review', 'failed_logins');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    $element['findings']['descriptions'][] = t('The following IPs were observed with an abundanced of failed login attempts.');
    if (is_null($results)) {
      $results = security_review_check_failed_logins();
    }
    foreach ($results['value'] as $ip) {
      $element['findings']['items'][] = array(
        'safe' => check_plain($ip),
        'raw' => $ip,
      );
    }
  }
  return $element;
}

/**
 * Look for admin permissions granted to untrusted roles.
 */
function security_review_check_admin_permissions() {
  $result = TRUE;
  $check_result_value = array();
  $untrusted_roles = security_review_untrusted_roles();
  $admin_perms = security_review_admin_permissions();
  // Get permissions for untrusted roles.
  $role_permissions = user_role_permissions($untrusted_roles);
  foreach ($role_permissions as $rid => $permissions) {
    $permissions = array_keys($permissions);
    $intersect = array_intersect($admin_perms, $permissions);
    if (!empty($intersect)) {
      $check_result_value[$rid] = $intersect;
    }
  }

  if (!empty($check_result_value)) {
    $result = FALSE;
  }
  return array('result' => $result, 'value' => $check_result_value);
}

function security_review_check_admin_permissions_help($results = NULL) {
  $element['title'] = t('Admin permissions');
  $element['descriptions'][] = t("Drupal's permission system is extensive and allows for varying degrees of control. Certain permissions would allow a user total control, or the ability to escalate their control, over your site and should only be granted to trusted users.");
  $element['descriptions'][] = t('<a href="!link">Read more about trusted vs. untrusted roles and permissions on DrupalScout.com.</a>', array('!link' => url('http://drupalscout.com/knowledge-base/importance-user-roles-and-permissions-site-security')));
  $last_check = security_review_get_last_check('security_review', 'admin_permissions');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    if (is_null($results)) {
      $results = security_review_check_admin_permissions();
    }
    $roles = user_roles();
    $element['findings']['descriptions'][] = t('You have granted untrusted roles the following administrative permissions that you should revoke.');
    foreach ($results['value'] as $rid => $permissions) {
      $permissions = implode(', ', $permissions);
      $html = t('<a href="!link">@name</a> has %permissions', array('!link' => url('admin/people/permissions/' . $rid), '@name' => $roles[$rid], '%permissions' => $permissions));
      $safe = t('@name has %permissions', array('@name' => $roles[$rid], $permissions));
      $element['findings']['items'][] = array(
        'html' => $html,
        'safe' => $safe,
        'raw' => $roles[$rid] . ':' . $permissions,
      );
    }
  }
  return $element;
}

function security_review_check_field($last_check = NULL) {
  $check_result = TRUE;
  $check_result_value = $tables = $found = array();
  $timestamp = NULL;
  $instances = field_info_instances();
  // Loop through instances checking for fields of type text.
  foreach ($instances as $entity_type => $type_bundles) {
    foreach ($type_bundles as $bundle => $bundle_instances) {
      foreach ($bundle_instances as $field_name => $instance) {
        $field = field_info_field($field_name);
        // Check into text fields that are stored in SQL.
        if ($field['module'] == 'text' && $field['storage']['module'] == 'field_sql_storage') {
          // Build array of tables and columns to search.
          $current_table = key($field['storage']['details']['sql'][FIELD_LOAD_CURRENT]);
          $revision_table = key($field['storage']['details']['sql'][FIELD_LOAD_REVISION]);
          if (!array_key_exists($current_table, $tables)) {
            $tables[$current_table] = $field['storage']['details']['sql'][FIELD_LOAD_CURRENT][$current_table]['value'];
          }
          if (!array_key_exists($revision_table, $tables)) {
            $tables[$revision_table] = $field['storage']['details']['sql'][FIELD_LOAD_REVISION][$revision_table]['value'];
          }
        }
      }
    }
  }
  if (empty($tables)) {
    return array('result' => $check_result, 'value' => $check_result_value);
  }
  // Search for PHP or Javascript tags in text columns.
  foreach ($tables as $table => $column) {
    $sql = "SELECT DISTINCT entity_id, entity_type FROM {" . $table . "} WHERE " . $column . " LIKE :text";
    // Handle changed? @todo
    foreach (array('Javascript' => '%<script%', 'PHP' => '%<?php%') as $description => $comparison) {
      $results = db_query($sql, array(':text' => $comparison)); // @pager query?
      foreach ($results as $result) {
        $check_result = FALSE;
        if (!isset($check_result_value[$result->entity_type]) || !array_key_exists($result->entity_id, $check_result_value[$result->entity_type])) {
          $check_result_value[$result->entity_type][$result->entity_id] = $description;
        }
      }
    }
  }

  return array('result' => $check_result, 'value' => $check_result_value);
}

function security_review_check_field_help($results = NULL) {
  $element['title'] = t('Dangerous tags in content');
  $element['descriptions'][] = t("Script and PHP code in content does not align with Drupal best practices and may be a vulnerability if an untrusted user is allowed to edit such content. It is recommended you remove such contents.");

  $last_check = security_review_get_last_check('security_review', 'field');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    $element['findings']['descriptions'][] = t('The following items potentially have dangerous tags.  ');
    if (is_null($results)) {
      $results = security_review_check_field(); // Don't pass $last_check because timestamp is wrong now.
    }
    foreach ($results['value'] as $entity_type => $value) {
      $ids = array_keys($value);
      $entity_info = entity_get_info($entity_type);
      $id = $entity_info['entity keys']['id'];
      $label = $entity_info['entity keys']['label'];
      $uri_callback = $entity_info['uri callback'];
      // There is no access checking. We state that the use of this module should be granted to trusted users only.
      $entities = entity_load($entity_type, $ids);
      foreach ($entities as $entity) {
        $uri = '#';
        if (function_exists($uri_callback)) {
          $uri = $uri_callback($entity);
          $uri = url($uri['path'] . '/edit'); // @todo can this assumption be made?
        }
        $html = t('@description found in <a href="!link">@title</a>', array('@description' => $value[$entity->{$id}], '!link' => $uri, '@title' => $entity->{$label}));
        $element['findings']['items'][] = array(
          'html' => $html,
          'safe' => t('@description in !url', array('@description' => $value[$entity->{$id}], '!url' => $uri)),
          'raw' => $value[$entity->{$id}] . ':' . $uri,
        );
      }
    }
    //$element['findings']['pager'] = theme('pager', array('tags' => NULL));
  }

  return $element;
}

function security_review_check_upload_extensions($last_check = NULL) {
  $check_result = TRUE;
  $check_result_value = array();
  $instances = field_info_instances();
  $unsafe_extensions = security_review_unsafe_extensions();
  // Loop through instances checking for fields of file.
  foreach ($instances as $entity_type => $type_bundles) {
    foreach ($type_bundles as $bundle => $bundle_instances) {
      foreach ($bundle_instances as $field_name => $instance) {
        $field = field_info_field($field_name);
        if ($field['module'] == 'image' || $field['module'] == 'file') {
          // Check instance file_extensions.
          foreach ($unsafe_extensions as $unsafe_extension) {
            if (strpos($instance['settings']['file_extensions'], $unsafe_extension) !== FALSE) {
              // Found an unsafe extension.
              $check_result_value[$instance['field_name']][] = $unsafe_extension;
              $check_result = FALSE;
            }
          }
        }
      }
    }
  }

  return array('result' => $check_result, 'value' => $check_result_value);
}

function security_review_check_upload_extensions_help($results = NULL) {
  $element['title'] = t('Allowed upload extensions');
  $element['descriptions'][] = t("File and image fields allow for uploaded files. Some extensions are considered dangerous because the files can be evaluated and then executued in the browser. A malicious user could use this opening to gain control of your site.");
  $last_check = security_review_get_last_check('security_review', 'upload_extensions');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif (TRUE || $last_check['result'] == '0') {
    if (is_null($results)) {
      $result = security_review_check_upload_extensions();
    }

    $element['findings']['descriptions'][] = t('The following extensions are considered unsafe and should be removed or limited from use. Or, be sure you are not granting untrusted users the ability to upload files.');
    foreach ($result['value'] as $field_name => $extensions) {
      foreach ($extensions as $extension) {
        $element['findings']['items'][] = array(
          'raw' => $extension,
          'safe' => check_plain($extension),
        );
      }
    }
  }
  return $element;
}

function security_review_check_name_passwords($last_check = NULL) {
  $result = TRUE;
  $check_result_value = array();
  $timestamp = NULL;

  // Check whether trusted roles have weak passwords.
  $trusted_roles = security_review_trusted_roles();
  if (!empty($trusted_roles)) {
    $trusted_roles = array_keys($trusted_roles);
    $check_result_value = _security_review_weak_passwords($trusted_roles);
  }
  if (!empty($check_result_value)) {
    $result = FALSE;
  }

  return array('result' => $result, 'value' => $check_result_value);
}

function _security_review_weak_passwords($trusted_roles) {
  $weak_users = array();

  // Select users with a trusted role whose password is their username.
  // @todo need to generate passwords in PHP to get salt.
  $sql = "SELECT u.uid, u.name, COUNT(rid) AS count FROM {users} u LEFT JOIN
    {users_roles} ur ON u.uid = ur.uid AND ur.rid in (:rids)
    WHERE pass = md5(name) GROUP BY uid";
  $results = db_query($sql, array(':rids' => $trusted_roles)); // @todo pager_query?
  foreach ($results as $row) {
    $record[] = $row;
    if ($row->count > 0) {
      $weak_users[$row->uid] = $row->name;
    }
  }

  // Explicitly check uid 1 in case they have no roles.
  $weak_uid1 = db_fetch_object(db_query("SELECT u.uid, u.name, 1 AS count FROM {users} u WHERE pass = md5(name) AND uid = 1"));
  if (!empty($weak_uid1->count)) {
    $weak_users[$weak_uid1->uid] = $weak_uid1->name; 
  }

  return $weak_users;
}

function security_review_check_name_passwords_help($results = NULL) {
  $element['title'] = t('Username as password');
  $element['descriptions'][] = t("Users with elevated access on the site (trusted users) who have a their account password the same as their username. It is recommended you enforce a password strength policy to avoid an attacker easily gaining access to your site.");

  $last_check = security_review_get_last_check('security_review', 'name_passwords');
  if ($last_check['skip'] == '1') {
    $element['findings']['descriptions'][] = _security_review_check_skipped($last_check);
  }
  elseif ($last_check['result'] == '0') {
    $element['findings']['descriptions'][] = t('The following users have extremely weak passwords. The links go to the edit page.');
    if (is_null($results)) {
      $results = security_review_check_name_passwords(); // Don't pass $last_check because timestamp is wrong now.
    }
    foreach ($results['value'] as $uid => $name) {
      $element['findings']['items'][] = array(
        'html' => l($name, 'user/' . $uid . '/edit'),
        'safe' => check_plain($name),
        'raw' => $name,
      );
    }
    $element['findings']['pager'] = theme('pager', NULL, 20);
  }

  return $element;
}

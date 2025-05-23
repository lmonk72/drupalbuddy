<?php
/**
 * @file
 * Admin/report page for DrupalBuddy.
 */

/**
 * Page callback for the DrupalBuddy report.
 */
function drupalbuddy_report_page() {
  $build = array();
  
  // Add local task tabs
  $build['tabs'] = array(
    '#theme' => 'menu_local_tasks',
  );
  
  $build['title'] = array(
    '#markup' => '<h2>' . t('DrupalBuddy: Unused Items Report') . '</h2>',
  );
  $build['desc'] = array(
    '#markup' => '<p>' . t('This page lists potentially unused Views, blocks, and modules to help you optimize your Drupal site.') . '</p>',
  );
  $build['cleanup_form'] = drupal_get_form('drupalbuddy_cleanup_orphaned_blocks_form');
  $build['views'] = array('#markup' => drupalbuddy_unused_views_report());
  $build['blocks'] = array('#markup' => drupalbuddy_unused_blocks_report());
  $build['modules'] = array('#markup' => drupalbuddy_unused_modules_report());
  return $build;
}

/**
 * Form to trigger orphaned block cleanup.
 */
function drupalbuddy_cleanup_orphaned_blocks_form($form, &$form_state) {
  $form['cleanup'] = array(
    '#type' => 'submit',
    '#value' => t('Clean Orphaned Blocks'),
    '#submit' => array('drupalbuddy_cleanup_orphaned_blocks_submit'),
  );
  return $form;
}

/**
 * Submit handler to remove orphaned blocks from the {block} table.
 */
function drupalbuddy_cleanup_orphaned_blocks_submit($form, &$form_state) {
  // Get all blocks of type 'views'.
  $result = db_query("SELECT module, delta, theme FROM {block} WHERE module = 'views'");
  $removed = 0;
  foreach ($result as $row) {
    // Delta format: view_name-block_display_id
    $parts = explode('-', $row->delta, 2);
    $view_name = $parts[0];
    $display_id = isset($parts[1]) ? $parts[1] : '';
    $view = views_get_view($view_name);
    // Remove if the view does not exist, or the block display does not exist.
    if (!$view || ($display_id && !isset($view->display[$display_id]))) {
      db_delete('block')
        ->condition('module', $row->module)
        ->condition('delta', $row->delta)
        ->condition('theme', $row->theme)
        ->execute();
      $removed++;
    }
  }
  drupal_set_message(t('DrupalBuddy removed @count orphaned view blocks.', array('@count' => $removed)));
}

/**
 * Helper: Find unused Views.
 */
function drupalbuddy_unused_views_report() {
  $header = array(t('View Name'), t('Status'));
  $rows = array();

  // Get all enabled views from {views_view}.
  if (module_exists('views')) {
    $result = db_query("SELECT name, human_name FROM {views_view} ORDER BY name ASC");
    foreach ($result as $row) {
      $view_name = $row->name;
      // Check if this view is used in a block or menu.
      $used = FALSE;
      // Check blocks.
      $block = db_query("SELECT * FROM {block} WHERE module = 'views' AND delta = :delta", array(':delta' => $view_name))->fetch();
      if ($block) {
        $used = TRUE;
      }
      // Check menu links.
      $menu = db_query("SELECT * FROM {menu_links} WHERE link_path LIKE :path", array(':path' => 'views/%' . $view_name . '%'))->fetch();
      if ($menu) {
        $used = TRUE;
      }
      $rows[] = array($view_name, $used ? t('Used') : t('Possibly unused'));
    }
  } else {
    $rows[] = array(t('Views module not enabled'), '');
  }
  return theme('table', array('header' => $header, 'rows' => $rows, 'caption' => t('Unused Views')));
}

/**
 * Helper: Find unused Blocks.
 */
function drupalbuddy_unused_blocks_report() {
  $header = array(t('Block'), t('Status'), t('Operations'));
  $rows = array();
  $theme = 'commsdealer';
  // Show blocks for the theme where status = 0 and region = '-1' (unassigned/disabled blocks).
  $result = db_query("SELECT module, delta, region FROM {block} WHERE theme = :theme AND status = 0 AND region = '-1' ORDER BY module ASC, delta ASC", array(':theme' => $theme));
  foreach ($result as $row) {
    $block_name = $row->module . ':' . $row->delta;
    // Instead of showing region '-1', display 'Unused'.
    $region = ($row->region === '-1') ? t('Unused') : $row->region;
    // Build the edit and delete links for the block config page.
    $edit_link = l(t('Edit'), 'admin/structure/block/manage/' . $row->module . '/' . $row->delta . '/configure', array('query' => array('theme' => $theme)));
    $delete_link = l(t('Delete'), 'admin/structure/block/manage/' . $row->module . '/' . $row->delta . '/delete', array('query' => array('theme' => $theme)));
    $ops = $edit_link . ' | ' . $delete_link;
    $rows[] = array($block_name, $region, $ops);
  }
  return theme('table', array('header' => $header, 'rows' => $rows, 'caption' => t('Unused blocks for theme: @theme', array('@theme' => $theme))));
}

/**
 * Helper: Find unused Modules.
 */
function drupalbuddy_unused_modules_report() {
  $header = array(t('Module'), t('Status'));
  $rows = array();
  $enabled = module_list();
  $enabled_sorted = $enabled;
  sort($enabled_sorted, SORT_STRING | SORT_FLAG_CASE);
  foreach ($enabled_sorted as $module) {
    // Skip core modules.
    if (in_array($module, array('system', 'user', 'field', 'filter', 'node', 'block', 'views', 'views_ui'))) continue;
    // Check if module provides blocks, views, or menu items.
    $provides = FALSE;
    // Blocks.
    $block = db_query("SELECT * FROM {block} WHERE module = :module", array(':module' => $module))->fetch();
    if ($block) $provides = TRUE;
    // Views.
    $view = db_query("SELECT * FROM {views_view} WHERE name LIKE :name", array(':name' => '%' . $module . '%'))->fetch();
    if ($view) $provides = TRUE;
    // Menu items.
    $menu = db_query("SELECT * FROM {menu_links} WHERE module = :module", array(':module' => $module))->fetch();
    if ($menu) $provides = TRUE;
    $rows[] = array($module, $provides ? t('Provides functionality') : t('Possibly unused'));
  }
  return theme('table', array('header' => $header, 'rows' => $rows, 'caption' => t('Unused Modules')));
}

/**
 * Page callback for exporting all webform registrations data with webform title.
 */
function drupalbuddy_webform_export_page() {
  // Check if webform module is enabled
  if (!module_exists('webform')) {
    return array('#markup' => '<p>' . t('Webform module is not enabled.') . '</p>');
  }
  
  // Load webform includes
  module_load_include('inc', 'webform', 'includes/webform.submissions');
  
  $build = array();
  $build['title'] = array(
    '#markup' => '<h2>' . t('Export All Webform Registrations') . '</h2>',
  );
  $build['description'] = array(
    '#markup' => '<p>' . t('Click the button below to export all webform registrations data, including the webform title for each record. The export will include all form fields and submission details in CSV format.') . '</p>',
  );
  $build['form'] = drupal_get_form('drupalbuddy_webform_export_form');
  
  return $build;
}

/**
 * Form for exporting webform registrations.
 */
function drupalbuddy_webform_export_form($form, &$form_state) {
  $form['export'] = array(
    '#type' => 'submit',
    '#value' => t('Export Webform Registrations'),
    '#submit' => array('drupalbuddy_webform_export_form_submit'),
  );
  return $form;
}

/**
 * Submit handler for exporting webform registrations.
 */
function drupalbuddy_webform_export_form_submit($form, &$form_state) {
  // Start the batch export process
  drupalbuddy_webform_export_batch();
}

/**
 * Start batch process to export all webform registrations.
 */
function drupalbuddy_webform_export_batch() {
  $batch = array(
    'title' => t('Exporting Webform Registrations...'),
    'operations' => array(
      array('drupalbuddy_export_webform_batch_process', array()),
    ),
    'finished' => 'drupalbuddy_export_webform_batch_finished',
    'file' => drupal_get_path('module', 'drupalbuddy') . '/drupalbuddy.admin.inc',
  );
  
  batch_set($batch);
  batch_process();
}

/**
 * Batch process callback for exporting webform data.
 */
function drupalbuddy_export_webform_batch_process(&$context) {
  // Load webform includes
  module_load_include('inc', 'webform', 'includes/webform.submissions');
  
  if (empty($context['sandbox'])) {
    // Initialize the sandbox
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_node'] = 0;
    $context['sandbox']['max'] = 0;
    
    // Get all nodes that have webforms
    $query = db_select('node', 'n')
      ->fields('n', array('nid', 'title'))
      ->condition('n.status', 1); // Only published nodes
    
    // Join with webform table to ensure node has a webform
    $query->join('webform', 'w', 'n.nid = w.nid');
    
    $result = $query->execute();
    $context['sandbox']['webform_nodes'] = array();
    
    foreach ($result as $row) {
      $context['sandbox']['webform_nodes'][$row->nid] = $row->title;
      $context['sandbox']['max']++;
    }
    
    if ($context['sandbox']['max'] == 0) {
      $context['results']['error'] = t('No webform nodes found.');
      $context['finished'] = 1;
      return;
    }
    
    // Initialize CSV data array
    $context['results']['csv_data'] = array();
    $context['results']['headers_set'] = FALSE;
  }
  
  // Process nodes in batches
  $limit = 5; // Process 5 nodes at a time
  $webform_nodes = $context['sandbox']['webform_nodes'];
  $node_ids = array_keys($webform_nodes);
  $current_batch = array_slice($node_ids, $context['sandbox']['progress'], $limit);
  
  foreach ($current_batch as $nid) {
    $node_title = $webform_nodes[$nid];
    
    // Load the node to get webform structure
    $node = node_load($nid);
    if (!$node || empty($node->webform)) {
      $context['sandbox']['progress']++;
      continue;
    }
    
    // Get all submissions for this webform
    $submissions = webform_get_submissions($nid);
    
    foreach ($submissions as $submission) {
      $row_data = array();
      $row_data['Webform Title'] = $node_title;
      $row_data['Submission ID'] = $submission->sid;
      $row_data['Submitted'] = date('Y-m-d H:i:s', $submission->submitted);
      $row_data['User'] = !empty($submission->name) ? $submission->name : 'Anonymous';
      $row_data['IP Address'] = $submission->remote_addr;
      
      // Add form component data
      if (!empty($submission->data) && !empty($node->webform['components'])) {
        foreach ($node->webform['components'] as $cid => $component) {
          $component_name = $component['name'];
          $component_value = '';
          
          if (isset($submission->data[$cid])) {
            if (is_array($submission->data[$cid])) {
              $component_value = implode(', ', $submission->data[$cid]);
            } else {
              $component_value = $submission->data[$cid];
            }
          }
          
          $row_data[$component_name] = $component_value;
        }
      }
      
      $context['results']['csv_data'][] = $row_data;
    }
    
    $context['sandbox']['progress']++;
  }
  
  // Update progress
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Batch finished callback for webform export.
 */
function drupalbuddy_export_webform_batch_finished($success, $results, $operations) {
  if ($success) {
    if (isset($results['error'])) {
      drupal_set_message($results['error'], 'error');
      return;
    }
    
    if (empty($results['csv_data'])) {
      drupal_set_message(t('No webform submissions found to export.'), 'warning');
      return;
    }
    
    // Generate CSV file
    $csv_data = $results['csv_data'];
    $filename = 'webform_registrations_export_' . date('Y-m-d_H-i-s') . '.csv';
    $file_path = file_default_scheme() . '://exports';
    
    // Ensure the exports directory exists
    file_prepare_directory($file_path, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);
    $full_path = $file_path . '/' . $filename;
    
    // Create CSV content
    $csv_content = '';
    $headers = array();
    
    // Get all unique headers from all rows
    foreach ($csv_data as $row) {
      $headers = array_merge($headers, array_keys($row));
    }
    $headers = array_unique($headers);
    
    // Add header row
    $csv_content .= '"' . implode('","', $headers) . '"' . "\n";
    
    // Add data rows
    foreach ($csv_data as $row) {
      $csv_row = array();
      foreach ($headers as $header) {
        $value = isset($row[$header]) ? $row[$header] : '';
        // Escape quotes and wrap in quotes
        $value = str_replace('"', '""', $value);
        $csv_row[] = $value;
      }
      $csv_content .= '"' . implode('","', $csv_row) . '"' . "\n";
    }
      // Save the file temporarily
    $file = file_save_data($csv_content, $full_path, FILE_EXISTS_REPLACE);
    
    if ($file) {
      // Set session message for users who might get redirected back
      $_SESSION['drupalbuddy_export_message'] = t('Export completed successfully! @count submissions exported.', array(
        '@count' => count($csv_data),
      ));
      
      // Force download of the generated file
      drupal_add_http_header('Content-Type', 'text/csv; utf-8');
      drupal_add_http_header('Content-Disposition', 'attachment; filename=' . $filename);
      drupal_add_http_header('Content-Length', strlen($csv_content));
      drupal_add_http_header('Cache-Control', 'must-revalidate, post-check=0, pre-check=0');
      drupal_add_http_header('Pragma', 'public');
      drupal_add_http_header('Expires', '0');
      
      // Output file content and end the request
      print $csv_content;
      drupal_exit();
    } else {
      drupal_set_message(t('Error creating export file.'), 'error');
    }
  } else {
    drupal_set_message(t('An error occurred during the export process.'), 'error');
  }
}

/**
 * Page callback for the Webform Bulk Remove tool.
 * Provides a form to upload a CSV and process removals/bans.
 */
function drupalbuddy_webform_bulk_remove_page() {
  $form = drupal_get_form('drupalbuddy_webform_bulk_remove_form');
  return array(
    'title' => array('#markup' => '<h2>' . t('Webform Bulk Remove & Ban') . '</h2>'),
    'desc' => array('#markup' => '<p>' . t('Upload a CSV file with columns SID and IP Address. Each IP will be banned and the webform submission with the given SID will be deleted.') . '</p>'),
    'form' => $form,
  );
}

/**
 * Form for uploading the CSV file for bulk removal/ban.
 */
function drupalbuddy_webform_bulk_remove_form($form, &$form_state) {
  $form = array();
  // Required for file uploads
  $form['#attributes'] = array('enctype' => 'multipart/form-data');
  
  $form['csv_file'] = array(
    '#type' => 'file',
    '#title' => t('CSV File'),
    '#description' => t('Upload a CSV file with columns: SID, IP Address.'),
  );
  
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Process CSV'),
  );
  
  return $form;
}

/**
 * Validate handler for the CSV upload form.
 */
function drupalbuddy_webform_bulk_remove_form_validate($form, &$form_state) {
  // Check if a file was uploaded
  if (empty($_FILES['files']['name']['csv_file'])) {
    form_set_error('csv_file', t('Please upload a CSV file.'));
    return;
  }
  
  // Validate file extension
  $validators = array('file_validate_extensions' => array('csv'));
  $file = file_save_upload('csv_file', $validators, FALSE, FILE_EXISTS_REPLACE);
  
  if (!$file) {
    form_set_error('csv_file', t('The file must be a CSV file.'));
    return;
  }
  
  // Store the file for use in submit handler
  $form_state['storage']['csv_file'] = $file;
}

/**
 * Form submit handler for bulk remove/ban.
 */
function drupalbuddy_webform_bulk_remove_form_submit($form, &$form_state) {
  // Get the stored file from validation
  if (isset($form_state['storage']['csv_file'])) {
    $file = $form_state['storage']['csv_file'];
    $filepath = drupal_realpath($file->uri);
    
    $handle = fopen($filepath, 'r');
    if ($handle) {
      $header = fgetcsv($handle);
      $sid_idx = array_search('SID', $header);
      $ip_idx = array_search('IP Address', $header);
      
      if ($sid_idx === FALSE || $ip_idx === FALSE) {
        drupal_set_message(t('CSV must contain columns: SID and IP Address.'), 'error');
        fclose($handle);
        file_delete($file);
        return;
      }      $banned = 0;
      $deleted = 0;
      while (($row = fgetcsv($handle)) !== FALSE) {
        // Skip empty rows
        if (empty($row[$sid_idx]) || empty($row[$ip_idx])) {
          continue;
        }
        
        $sid = trim($row[$sid_idx]);
        $ip = trim($row[$ip_idx]);        // Ban the IP (using Drupal 7's System module IP blocking)
        if (!empty($ip)) {
          // Check if the blocked_ips table exists
          if (db_table_exists('blocked_ips')) {
            // Check if IP is already blocked
            if (!db_query("SELECT 1 FROM {blocked_ips} WHERE ip = :ip", array(':ip' => $ip))->fetchField()) {
              // Block the IP
              db_insert('blocked_ips')
                ->fields(array(
                  'ip' => $ip,
                ))
                ->execute();
              $banned++;
            }
          } else {
            drupal_set_message(t('The blocked_ips table does not exist. IPs will not be banned.'), 'warning');
          }
        }
        
        // Delete the webform submission by SID
        if (!empty($sid) && module_exists('webform')) {
          module_load_include('inc', 'webform', 'includes/webform.submissions');
          $submission = webform_get_submission(NULL, $sid);
          if ($submission) {
            webform_submission_delete($submission->nid, $submission->sid);
            $deleted++;
          }
        }
      }
      fclose($handle);
      
      // Remove the temporary file
      file_delete($file);
      
      // Display results
      drupal_set_message(t('Successfully processed CSV: @banned IP addresses banned and @deleted submissions deleted.', array(
        '@banned' => $banned,
        '@deleted' => $deleted,
      )));
    } 
    else {
      drupal_set_message(t('Could not open uploaded file.'), 'error');
      file_delete($file);
    }
  }
  else {
    drupal_set_message(t('No file was uploaded or there was an error with the upload.'), 'error');
  }
}

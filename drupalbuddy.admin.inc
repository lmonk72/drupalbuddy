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
      }
      
      $banned = 0;
      $deleted = 0;
      while (($row = fgetcsv($handle)) !== FALSE) {
        // Skip empty rows
        if (empty($row[$sid_idx]) || empty($row[$ip_idx])) {
          continue;
        }
        
        $sid = trim($row[$sid_idx]);
        $ip = trim($row[$ip_idx]);
        
        // Ban the IP (using Drupal 7's System module IP blocking)
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
          } 
          else {
            drupal_set_message(t('The blocked_ips table does not exist. IPs will not be banned.'), 'warning');
          }
        }
        
        // Delete the webform submission by SID
        if (!empty($sid) && module_exists('webform')) {
          module_load_include('inc', 'webform', 'includes/webform.submissions');
          
          // Check if SID is numeric
          if (!is_numeric($sid)) {
            watchdog('drupalbuddy', 'Invalid SID format: @sid', array('@sid' => $sid), WATCHDOG_WARNING);
            continue;
          }
          
          // First, get the node ID associated with this submission
          // We need to query this separately because webform_get_submission requires both nid and sid
          $submission_data = db_query("SELECT nid FROM {webform_submissions} WHERE sid = :sid", 
            array(':sid' => $sid))->fetchField();
          
          if ($submission_data) {
            $nid = $submission_data;
            
            // Check if the node exists and is a webform
            $node = node_load($nid);
            if ($node && !empty($node->webform)) {
              try {
                // Load submission properly using the Webform API with the correct nid
                $submission = webform_get_submission($nid, $sid);
                
                if ($submission && isset($submission->sid)) {
                  // Delete using Webform's proper API function - pass node and submission objects
                  webform_submission_delete($node, $submission);
                  $deleted++;
                  
                  // Log successful deletion
                  watchdog('drupalbuddy', 'Deleted webform submission: @sid from form #@nid', 
                    array('@sid' => $sid, '@nid' => $nid), WATCHDOG_INFO);
                }
                else {
                  // Submission could not be loaded properly, use direct database approach
                  db_delete('webform_submitted_data')->condition('sid', $sid)->execute();
                  db_delete('webform_submissions')->condition('sid', $sid)->execute();
                  $deleted++;
                  
                  watchdog('drupalbuddy', 'Deleted webform submission directly from database: @sid', 
                    array('@sid' => $sid), WATCHDOG_INFO);
                }
              } 
              catch (Exception $e) {
                // Log the exception message to help debugging
                watchdog('drupalbuddy', 'Exception during submission deletion: @message', 
                  array('@message' => $e->getMessage()), WATCHDOG_ERROR);
                  
                // If API approach fails, try direct database deletion as final fallback
                try {
                  db_delete('webform_submitted_data')->condition('sid', $sid)->execute();
                  db_delete('webform_submissions')->condition('sid', $sid)->execute();
                  $deleted++;
                  
                  watchdog('drupalbuddy', 'Failed with API but deleted via direct database query: @sid', 
                    array('@sid' => $sid), WATCHDOG_WARNING);
                }
                catch (Exception $e2) {
                  // If even direct database deletion fails, log the error
                  watchdog('drupalbuddy', 'Failed to delete submission @sid: @message', 
                    array('@sid' => $sid, '@message' => $e2->getMessage()), WATCHDOG_ERROR);
                }
              }
            }
            else {
              // Node doesn't exist, but we can still delete the submission from DB
              db_delete('webform_submitted_data')->condition('sid', $sid)->execute();
              db_delete('webform_submissions')->condition('sid', $sid)->execute();
              $deleted++;
              
              watchdog('drupalbuddy', 'Node not found but deleted submission from database: @sid', 
                array('@sid' => $sid), WATCHDOG_WARNING);
            }
          }
          else {
            // Could not find the nid for this submission
            watchdog('drupalbuddy', 'Could not find node ID for webform submission with SID: @sid', 
              array('@sid' => $sid), WATCHDOG_WARNING);
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

/**
 * Page callback for the Content Migration tool.
 * Page callback for content migration.
 *
 * Provides a form to migrate content between content types with field mapping.
 */
function drupalbuddy_content_migration_page() {
  $form = drupal_get_form('drupalbuddy_content_migration_form');
  return array(
    'title' => array('#markup' => '<h2>' . t('Content Migration') . '</h2>'),
    'desc' => array('#markup' => '<p>' . t('Migrate content from one content type to another using a CSV field mapping file. The CSV should have columns: <strong>source_field</strong>, <strong>target_field</strong>, <strong>action</strong> (where action can be "map" or "ignore").') . '</p>' .
      '<p>' . t('Sample CSV template files are available in the module directory: <code>content_migration_mapping_template.csv</code> and <code>basic_migration_example.csv</code>') . '</p>'),
    'form' => $form,
  );
}

/**
 * Form for content migration with field mapping.
 */
function drupalbuddy_content_migration_form($form, &$form_state) {
  $form = array();
  
  // Get all available content types
  $content_types = node_type_get_types();
  $type_options = array();
  foreach ($content_types as $machine_name => $type) {
    $type_options[$machine_name] = $type->name;
  }
  
  $form['source_type'] = array(
    '#type' => 'select',
    '#title' => t('Source Content Type'),
    '#description' => t('Select the content type to migrate from.'),
    '#options' => $type_options,
    '#required' => TRUE,
  );
    $form['target_type'] = array(
    '#type' => 'select',
    '#title' => t('Target Content Type'),
    '#description' => t('Select the content type to migrate to.'),
    '#options' => $type_options,
    '#required' => TRUE,
  );
  
  $form['migration_scope'] = array(
    '#type' => 'radios',
    '#title' => t('Migration Scope'),
    '#description' => t('Choose whether to migrate all content or specific nodes.'),
    '#options' => array(
      'all' => t('Migrate all nodes of the source content type'),
      'specific' => t('Migrate only specific node IDs'),
    ),
    '#default_value' => 'all',
    '#required' => TRUE,
  );
  
  $form['specific_nids'] = array(
    '#type' => 'textarea',
    '#title' => t('Specific Node IDs'),
    '#description' => t('Enter the node IDs to migrate, one per line or separated by commas. Example: 123, 456, 789 or one per line.'),
    '#rows' => 5,
    '#states' => array(
      'visible' => array(
        ':input[name="migration_scope"]' => array('value' => 'specific'),
      ),
      'required' => array(
        ':input[name="migration_scope"]' => array('value' => 'specific'),
      ),
    ),
  );
  
  // Required for file uploads
  $form['#attributes'] = array('enctype' => 'multipart/form-data');
  
  $form['mapping_file'] = array(
    '#type' => 'file',
    '#title' => t('Field Mapping CSV File'),
    '#description' => t('Upload a CSV file with columns: source_field, target_field, action. Action should be "map" for fields to migrate or "ignore" for fields to skip.'),
  );
    $form['preview'] = array(
    '#type' => 'submit',
    '#value' => t('Preview Mapping'),
    '#submit' => array('drupalbuddy_content_migration_preview_submit'),
    '#attributes' => array('class' => array('button')),
  );
  
  $form['execute'] = array(
    '#type' => 'submit',
    '#value' => t('Execute Migration'),
    '#submit' => array('drupalbuddy_content_migration_execute_submit'),
    '#attributes' => array('class' => array('button-primary')),
    '#prefix' => '<div style="margin-top: 10px;"><strong>' . t('Warning:') . '</strong> ' . t('This will create new nodes and unpublish the original content. Make sure to preview first!') . '</div>',
  );
  
  // Show preview if available
  if (isset($form_state['storage']['preview_data'])) {
    $form['preview_results'] = array(
      '#markup' => $form_state['storage']['preview_data'],
      '#weight' => 10,
    );
  }
  
  return $form;
}

/**
 * Validate handler for the content migration form.
 */
function drupalbuddy_content_migration_form_validate($form, &$form_state) {
  // Check if source and target types are different
  if ($form_state['values']['source_type'] == $form_state['values']['target_type']) {
    form_set_error('target_type', t('Source and target content types must be different.'));
  }
  
  // Validate specific node IDs if selected
  if ($form_state['values']['migration_scope'] == 'specific') {
    $specific_nids = $form_state['values']['specific_nids'];
    
    if (empty($specific_nids)) {
      form_set_error('specific_nids', t('Please enter node IDs when using specific node migration.'));
      return;
    }
    
    // Parse node IDs (support both comma-separated and line-separated)
    $nids = array();
    $raw_nids = preg_split('/[\s,]+/', trim($specific_nids), -1, PREG_SPLIT_NO_EMPTY);
    
    foreach ($raw_nids as $nid) {
      $nid = trim($nid);
      if (!is_numeric($nid)) {
        form_set_error('specific_nids', t('All node IDs must be numeric. Invalid ID: @nid', array('@nid' => $nid)));
        return;
      }
      $nids[] = (int) $nid;
    }
    
    // Validate that nodes exist and are of the correct type
    $source_type = $form_state['values']['source_type'];
    $existing_nodes = db_query("SELECT nid FROM {node} WHERE nid IN (:nids) AND type = :type", 
      array(':nids' => $nids, ':type' => $source_type))->fetchCol();
    
    $missing_nodes = array_diff($nids, $existing_nodes);
    if (!empty($missing_nodes)) {
      form_set_error('specific_nids', t('The following node IDs do not exist or are not of type @type: @nids', 
        array('@type' => $source_type, '@nids' => implode(', ', $missing_nodes))));
      return;
    }
    
    // Store validated node IDs
    $form_state['storage']['validated_nids'] = $nids;
  }
  
  // Check if a file was uploaded
  if (empty($_FILES['files']['name']['mapping_file'])) {
    form_set_error('mapping_file', t('Please upload a CSV mapping file.'));
    return;
  }
  
  // Validate file extension
  $validators = array('file_validate_extensions' => array('csv'));
  $file = file_save_upload('mapping_file', $validators, FALSE, FILE_EXISTS_REPLACE);
  
  if (!$file) {
    form_set_error('mapping_file', t('The file must be a CSV file.'));
    return;
  }
  
  // Store the file for use in submit handler
  $form_state['storage']['mapping_file'] = $file;
}

/**
 * Submit handler for preview mapping.
 */
function drupalbuddy_content_migration_preview_submit($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  
  if (isset($form_state['storage']['mapping_file'])) {
    $file = $form_state['storage']['mapping_file'];
    $filepath = drupal_realpath($file->uri);
    
    $handle = fopen($filepath, 'r');
    if ($handle) {
      $header = fgetcsv($handle);
      $source_idx = array_search('source_field', $header);
      $target_idx = array_search('target_field', $header);
      $action_idx = array_search('action', $header);
      
      if ($source_idx === FALSE || $target_idx === FALSE || $action_idx === FALSE) {
        drupal_set_message(t('CSV must contain columns: source_field, target_field, action.'), 'error');
        fclose($handle);
        return;
      }
      
      $source_type = $form_state['values']['source_type'];
      $target_type = $form_state['values']['target_type'];
        // Get field info for both content types
      $source_fields = drupalbuddy_get_content_type_fields($source_type);
      $target_fields = drupalbuddy_get_content_type_fields($target_type);
      
      // Count nodes to migrate based on scope
      if ($form_state['values']['migration_scope'] == 'specific' && isset($form_state['storage']['validated_nids'])) {
        $source_count = count($form_state['storage']['validated_nids']);
        $scope_text = t('specific nodes: ') . implode(', ', $form_state['storage']['validated_nids']);
      } else {
        $source_count = db_query("SELECT COUNT(*) FROM {node} WHERE type = :type", 
          array(':type' => $source_type))->fetchField();
        $scope_text = t('all nodes');
      }
      
      $preview_html = '<h3>' . t('Migration Preview') . '</h3>';
      $preview_html .= '<p><strong>' . t('Source:') . '</strong> ' . $source_type . ' (' . $source_count . ' ' . t('nodes') . ' - ' . $scope_text . ')</p>';
      $preview_html .= '<p><strong>' . t('Target:') . '</strong> ' . $target_type . '</p>';
      $preview_html .= '<h4>' . t('Field Mappings:') . '</h4>';
      
      $preview_html .= '<table class="table">';
      $preview_html .= '<thead><tr><th>' . t('Source Field') . '</th><th>' . t('Target Field') . '</th><th>' . t('Action') . '</th><th>' . t('Status') . '</th></tr></thead>';
      $preview_html .= '<tbody>';
      
      while (($row = fgetcsv($handle)) !== FALSE) {
        if (empty($row[$source_idx])) continue;
        
        $source_field = trim($row[$source_idx]);
        $target_field = trim($row[$target_idx]);
        $action = trim($row[$action_idx]);
        
        $status = '';
        $row_class = '';
        
        if ($action == 'ignore') {
          $status = t('Will be ignored');
          $row_class = 'warning';
        } elseif ($action == 'map') {
          if (!isset($source_fields[$source_field])) {
            $status = t('Source field not found');
            $row_class = 'error';
          } elseif (!isset($target_fields[$target_field])) {
            $status = t('Target field not found');
            $row_class = 'error';
          } else {
            $status = t('Ready to map');
            $row_class = 'success';
          }
        } else {
          $status = t('Invalid action');
          $row_class = 'error';
        }
        
        $preview_html .= '<tr class="' . $row_class . '"><td>' . $source_field . '</td><td>' . $target_field . '</td><td>' . $action . '</td><td>' . $status . '</td></tr>';
      }
      
      $preview_html .= '</tbody></table>';
      $preview_html .= '<style>.success{background-color:#dff0d8;} .warning{background-color:#fcf8e3;} .error{background-color:#f2dede;}</style>';
        $form_state['storage']['preview_data'] = $preview_html;
      
      // Store the mapping data for execute function
      $handle2 = fopen($filepath, 'r');
      $header2 = fgetcsv($handle2);
      $source_idx2 = array_search('source_field', $header2);
      $target_idx2 = array_search('target_field', $header2);
      $action_idx2 = array_search('action', $header2);
      
      $mapping_data = array();
      while (($row2 = fgetcsv($handle2)) !== FALSE) {
        if (empty($row2[$source_idx2])) continue;
        
        $source_field = trim($row2[$source_idx2]);
        $target_field = trim($row2[$target_idx2]);
        $action = trim($row2[$action_idx2]);
        
        if ($action == 'map') {
          $mapping_data[$source_field] = $target_field;
        }
      }
      fclose($handle2);
      $form_state['storage']['mapping_data'] = $mapping_data;
      
      fclose($handle);
    }
    
    // Clean up the file after preview
    file_delete($file);
    unset($form_state['storage']['mapping_file']);
  }
}

/**
 * Submit handler for execute migration.
 */
function drupalbuddy_content_migration_execute_submit($form, &$form_state) {
  if (isset($form_state['storage']['mapping_data'])) {
    $mapping_data = $form_state['storage']['mapping_data'];
    $source_type = $form_state['values']['source_type'];
    $target_type = $form_state['values']['target_type'];
    $migration_scope = $form_state['values']['migration_scope'];
    $specific_nids = ($migration_scope == 'specific' && isset($form_state['storage']['validated_nids'])) 
      ? $form_state['storage']['validated_nids'] 
      : array();
    
    // Set up batch process
    $batch = array(
      'title' => t('Migrating content from @source to @target', array(
        '@source' => $source_type, 
        '@target' => $target_type
      )),
      'operations' => array(
        array('drupalbuddy_content_migration_batch_process', array($mapping_data, $source_type, $target_type, $migration_scope, $specific_nids)),
      ),
      'finished' => 'drupalbuddy_content_migration_batch_finished',
      'file' => drupal_get_path('module', 'drupalbuddy') . '/drupalbuddy.admin.inc',
    );
    
    batch_set($batch);
    batch_process();
  } else {
    drupal_set_message(t('Please preview the mapping first before executing the migration.'), 'error');
  }
}

/**
 * Batch process callback for content migration.
 */
function drupalbuddy_content_migration_batch_process($mapping_data, $source_type, $target_type, $migration_scope, $specific_nids, &$context) {
  if (empty($context['sandbox'])) {
    // Initialize the sandbox
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_node'] = 0;
    
    // Use the passed mapping data directly
    $context['sandbox']['mapping'] = $mapping_data;
    
    // Get nodes to migrate based on scope
    if ($migration_scope == 'specific' && !empty($specific_nids)) {
      // Use specific node IDs
      $context['sandbox']['nodes'] = $specific_nids;
    } else {
      // Get all nodes of source type
      $query = db_select('node', 'n')
        ->fields('n', array('nid'))
        ->condition('n.type', $source_type)
        ->orderBy('n.nid');
      
      $result = $query->execute();
      $context['sandbox']['nodes'] = array();
      
      foreach ($result as $row) {
        $context['sandbox']['nodes'][] = $row->nid;
      }
    }
    
    $context['sandbox']['max'] = count($context['sandbox']['nodes']);
    $context['results']['migrated'] = 0;
    $context['results']['errors'] = 0;
  }
  
  // Process nodes in batches
  $limit = 5; // Process 5 nodes at a time
  $nodes = $context['sandbox']['nodes'];
  $current_batch = array_slice($nodes, $context['sandbox']['progress'], $limit);
  
  foreach ($current_batch as $nid) {
    try {
      $source_node = node_load($nid);
      if (!$source_node) {
        $context['results']['errors']++;
        continue;
      }
      
      // Create new node with target type
      $new_node = new stdClass();
      $new_node->type = $target_type;
      $new_node->uid = $source_node->uid;
      $new_node->created = $source_node->created;
      $new_node->changed = REQUEST_TIME;
      $new_node->status = $source_node->status;
      $new_node->comment = $source_node->comment;
      $new_node->promote = $source_node->promote;
      $new_node->sticky = $source_node->sticky;
      $new_node->language = $source_node->language;
      
      // Map fields according to mapping
      foreach ($context['sandbox']['mapping'] as $source_field => $target_field) {
        if (isset($source_node->$source_field)) {
          $new_node->$target_field = $source_node->$source_field;
        }
      }
      
      // Save the new node
      node_save($new_node);
      
      // Update the original node to unpublished to avoid duplicates
      $source_node->status = 0;
      node_save($source_node);
      
      $context['results']['migrated']++;
      
      watchdog('drupalbuddy', 'Migrated node @nid from @source to @target (new nid: @new_nid)', 
        array('@nid' => $nid, '@source' => $source_type, '@target' => $target_type, '@new_nid' => $new_node->nid), 
        WATCHDOG_INFO);
      
    } catch (Exception $e) {
      $context['results']['errors']++;
      watchdog('drupalbuddy', 'Error migrating node @nid: @message', 
        array('@nid' => $nid, '@message' => $e->getMessage()), WATCHDOG_ERROR);
    }
    
    $context['sandbox']['progress']++;
  }
  
  // Update progress
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Batch finished callback for content migration.
 */
function drupalbuddy_content_migration_batch_finished($success, $results, $operations) {
  if ($success) {
    drupal_set_message(t('Content migration completed successfully! @migrated nodes migrated, @errors errors.', array(
      '@migrated' => $results['migrated'],
      '@errors' => $results['errors'],
    )));
  } else {
    drupal_set_message(t('An error occurred during the migration process.'), 'error');
  }
}

/**
 * Helper function to get fields for a content type.
 */
function drupalbuddy_get_content_type_fields($content_type) {
  $fields = array();
  
  // Get base node fields
  $fields['nid'] = array('type' => 'node', 'label' => 'Node ID');
  $fields['title'] = array('type' => 'node', 'label' => 'Title');
  $fields['body'] = array('type' => 'field', 'label' => 'Body');
  $fields['path'] = array('type' => 'path', 'label' => 'URL path');
  
  // Get Field API fields
  $instances = field_info_instances('node', $content_type);
  foreach ($instances as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    $fields[$field_name] = array(
      'type' => 'field',
      'label' => $instance['label'],
      'field_type' => $field_info['type'],
    );
  }
  
  return $fields;
}

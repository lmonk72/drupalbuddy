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
    $result = db_query("SELECT name, human_name FROM {views_view}");
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
  $header = array(t('Block'), t('Region'), t('Operations'));
  $rows = array();
  $theme = 'commsdealer';
  // Show blocks for the theme where status = 0 and region = '-1' (unassigned/disabled blocks).
  $result = db_query("SELECT module, delta, region FROM {block} WHERE theme = :theme AND status = 0 AND region = '-1'", array(':theme' => $theme));
  foreach ($result as $row) {
    $block_name = $row->module . ':' . $row->delta;
    $region = $row->region;
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
  foreach ($enabled as $module) {
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

# Specific Node Migration Feature Update

## Overview
Enhanced the DrupalBuddy content migration feature to support migrating specific node IDs instead of just entire content types.

## New Features Added

### 1. Migration Scope Selection
- **All Nodes**: Migrate all nodes of the source content type (existing functionality)
- **Specific Nodes**: Migrate only specific node IDs entered by the user

### 2. Node ID Input Validation
- Supports multiple input formats:
  - Comma-separated: `123, 456, 789`
  - Line-separated: One node ID per line
  - Mixed whitespace and comma separation
- Validates that all entered IDs are numeric
- Verifies that node IDs exist and are of the correct content type
- Provides clear error messages for invalid or missing nodes

### 3. Enhanced Preview Functionality
- Shows the exact node IDs that will be migrated when using specific node selection
- Displays total count of nodes to be migrated for both modes
- Clear indication of migration scope in the preview

### 4. Updated Batch Processing
- Modified batch process to handle both migration modes
- Processes specific node IDs when selected
- Maintains existing functionality for full content type migration

## Usage Example

### For your 20 Article nodes:
1. Navigate to `admin/reports/drupalbuddy/content-migration`
2. Select "Article" as Source Content Type
3. Select "Whitepaper" as Target Content Type
4. Choose "Migrate only specific node IDs"
5. Enter your 20 node IDs (one per line or comma-separated)
6. Upload your CSV mapping file
7. Click "Preview Mapping" to verify
8. Click "Execute Migration" to run

### Node ID Input Examples:
```
123, 456, 789, 1001, 1002
```

or

```
123
456
789
1001
1002
```

## Technical Implementation

### Form Validation
```php
// Validates specific node IDs if selected
if ($form_state['values']['migration_scope'] == 'specific') {
  // Parse and validate node IDs
  // Check nodes exist and are correct type
  // Store validated IDs for processing
}
```

### Batch Processing Update
```php
// Get nodes to migrate based on scope
if ($migration_scope == 'specific' && !empty($specific_nids)) {
  // Use specific node IDs
  $context['sandbox']['nodes'] = $specific_nids;
} else {
  // Get all nodes of source type (existing logic)
}
```

## Benefits
- **Precise Control**: Migrate only the content you need
- **Testing Friendly**: Easy to test with small batches
- **Resource Efficient**: Avoid processing unnecessary content
- **Error Handling**: Clear validation prevents migration issues

## File Updated
- `drupalbuddy.admin.inc` - Enhanced with specific node ID functionality

The feature is now ready for testing with your specific 20 Article nodes migration to Whitepaper content type.

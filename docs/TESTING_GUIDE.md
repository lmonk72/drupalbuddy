# Testing Guide for DrupalBuddy Content Migration

## Quick Test Setup

### Prerequisites
1. DrupalBuddy module is enabled
2. You have "administer drupalbuddy" permission
3. You have at least two different content types with some content

### Test Steps

#### Step 1: Access the Migration Tool
1. Navigate to: `/admin/reports/drupalbuddy/content-migration`
2. You should see a form with:
   - Source Content Type dropdown
   - Target Content Type dropdown  
   - File upload field for CSV mapping
   - Preview and Execute buttons

#### Step 2: Quick Test with Basic Migration
1. Use the provided `basic_migration_example.csv` file
2. Select any content type as source (e.g., "article" if you have articles)
3. Select a different content type as target (e.g., "page")
4. Upload the CSV file
5. Click "Preview Mapping"

#### Step 3: Expected Preview Results
You should see:
- A table showing field mappings
- Status indicators (green=ready, yellow=warning, red=error)
- Count of nodes that will be migrated
- Clear status messages for each field mapping

#### Step 4: Execute Migration (Optional)
- If preview looks good, click "Execute Migration"
- Watch the batch process progress
- Check results in the target content type

## Detailed Test Scenarios

### Scenario 1: Article to Page Migration
**Purpose**: Test basic content migration between standard Drupal types

**Setup**:
1. Create a few "Article" nodes with title, body, and image
2. Create this CSV mapping:
```csv
source_field,target_field,action
title,title,map
body,body,map
field_image,field_image,map
field_tags,field_tags,ignore
```

**Expected Result**: Articles copied to Page content type with preserved content

### Scenario 2: Custom Field Migration
**Purpose**: Test migration between custom content types

**Setup**:
1. If you have custom content types with custom fields
2. Create CSV mapping between similar fields
3. Include some "ignore" actions for fields you don't want to migrate

**Expected Result**: Custom fields properly mapped according to CSV

### Scenario 3: Error Handling Test
**Purpose**: Test validation and error handling

**Setup**:
1. Create a CSV with invalid field names:
```csv
source_field,target_field,action
title,title,map
invalid_field,another_invalid,map
body,body,map
```

**Expected Result**: Preview shows red error status for invalid fields

## Verification Checklist

### ✅ Basic Functionality
- [ ] Menu item appears at `/admin/reports/drupalbuddy/content-migration`
- [ ] Form loads without errors
- [ ] Content type dropdowns populate correctly
- [ ] File upload field accepts CSV files
- [ ] File upload rejects non-CSV files

### ✅ Preview Function
- [ ] Preview button processes CSV correctly
- [ ] Shows field mapping table
- [ ] Displays correct status indicators
- [ ] Shows node count to be migrated
- [ ] Validates field existence
- [ ] Handles invalid CSV format gracefully

### ✅ Migration Execution
- [ ] Execute button starts batch process
- [ ] Batch progress shows correctly
- [ ] New nodes created in target content type
- [ ] Original nodes remain but unpublished
- [ ] Field data copied correctly
- [ ] Completion message shows migration stats

### ✅ Error Handling
- [ ] Invalid field names flagged in preview
- [ ] Missing CSV columns detected
- [ ] Same source/target type rejected
- [ ] Large migrations don't timeout

## Troubleshooting Common Issues

### Issue: "Function not found" errors
**Solution**: Clear Drupal cache: `drush cc all`

### Issue: CSV validation fails
**Check**:
- CSV has exactly 3 columns: source_field, target_field, action
- Action values are only "map" or "ignore" 
- No empty rows or malformed data

### Issue: Field mapping errors in preview
**Check**:
- Field names match exactly (case-sensitive)
- Custom fields include "field_" prefix
- Fields exist in both content types

### Issue: Migration timeout
**Solution**: Process smaller batches or increase PHP memory/time limits

## Sample Test Data

### Minimal Test CSV
```csv
source_field,target_field,action
title,title,map
body,body,map
```

### Complete Test CSV
```csv
source_field,target_field,action
title,title,map
body,body,map
field_image,field_image,map
field_tags,field_tags,map
created,created,map
status,status,map
promote,promote,ignore
sticky,sticky,ignore
field_custom1,field_custom2,map
field_unused,field_ignore,ignore
```

## Test Results Template

### Test Environment
- Drupal Version: ___
- PHP Version: ___
- Test Date: ___
- Tester: ___

### Test Results
- [ ] Module loads correctly
- [ ] Menu navigation works
- [ ] Form displays properly
- [ ] CSV upload functions
- [ ] Preview shows results
- [ ] Migration executes successfully
- [ ] Data integrity maintained
- [ ] Error handling works

### Notes
_Record any issues, observations, or suggestions here_

## Advanced Testing

### Performance Test
- Create 100+ nodes in source type
- Test migration performance
- Monitor memory usage
- Verify batch processing works correctly

### Data Integrity Test
- Migrate content with complex field types (references, files, etc.)
- Verify all field data preserves correctly
- Check entity references maintain links
- Verify file attachments copy properly

### Edge Case Testing
- Empty fields
- Very long content
- Special characters in field names
- Circular content references
- Missing field permissions

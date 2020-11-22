<?php

/**
 * Adds database references to existing Germplasm.
 */
class GermplasmDOI extends TripalImporter {


    // ------------------------------------------------------------------------
    //                     EDITABLE STATIC CONSTANTS
    //
    // The following constants SHOULD be set for each descendent class.  They
    // are used by the static functions to provide information to Drupal about
    // the field and it's default widget and formatter.
    // ------------------------------------------------------------------------

    /**
     * The name of this loader.  This name will be presented to the site
     * user.
     */
    public static $name = 'Germplasm DOI Importer';

    /**
     * The machine name for this loader. This name will be used to construct
     * the URL for the loader.
     */
    public static $machine_name = 'divseek_germplasm_doi';

    /**
     * A brief description for this loader.  This description will be
     * presented to the site user.
     */
    public static $description = 'Adds additional database accessions, synonyms  and digital object identifiers (DOI) to existing germplasm.';

    /**
     * An array containing the extensions of allowed file types.
     */
    public static $file_types = ['txt','tsv'];


    /**
     * Provides information to the user about the file upload.  Typically this
     * may include a description of the file types allowed.
     */
    public static $upload_description = 'The uploaded file should be a tab-separated text file consisting of the following columns:
    <ol>
      <li>Genus: The genus the existing germplasm is associated with.</li>
      <li>Species: The species the existing germplasm is associated with.</li>
      <li>Name: The name of the existing germplasm.</li>
      <li>Identifier: The unique identifier of the existing germplasm.</li>
      <li>Database Name: the unique name of the database the new accession is from. This should already exist; you can add them <a href="admin/tripal/loaders/chado_db/add">here</a>.</li>
      <li>Database Accession: The accession, synonym or DOI you would like to add.</li>
    </ol>
    The first four columns identify the existing germplasm and the remaining columns include the information you want to import. You will have one row in the file for each database accession, synonym, or digital object identifier (DOI) you want to add.';

    /**
     * The title that should appear above the upload button.
     */
    public static $upload_title = 'File Upload';

    /**
     * If the loader should require an analysis record.  To maintain provenance
     * we should always indicate where the data we are uploading comes from.
     * The method that Tripal attempts to use for this by associating upload files
     * with an analysis record.  The analysis record provides the details for
     * how the file was created or obtained. Set this to FALSE if the loader
     * should not require an analysis when loading. if $use_analysis is set to
     * true then the form values will have an 'analysis_id' key in the $form_state
     * array on submitted forms.
     */
    public static $use_analysis = FALSE;

    /**
     * If the $use_analysis value is set above then this value indicates if the
     * analysis should be required.
     */
    public static $require_analysis = FALSE;

    /**
     * Text that should appear on the button at the bottom of the importer
     * form.
     */
    public static $button_text = 'Import File';

    /**
     * Indicates the methods that the file uploader will support.
     */
    public static $methods = [
      // Allow the user to upload a file to the server.
      'file_upload' => TRUE,
      // Allow the user to provide the path on the Tripal server for the file.
      'file_local' => TRUE,
      // Allow the user to provide a remote URL for the file.
      'file_remote' => TRUE,
    ];

    /**
     * Indicates if the file must be provided.  An example when it may not be
     * necessary to require that the user provide a file for uploading if the
     * loader keeps track of previous files and makes those available for
     * selection.
     */
    public static $file_required = TRUE;


    /**
     * The array of arguments used for this loader.  Each argument should
     * be a separate array containing a machine_name, name, and description
     * keys.  This information is used to build the help text for the loader.
     */
    public static $argument_list = [];


    /**
     * Indicates how many files are allowed to be uploaded.  By default this is
     * set to allow only one file.  Change to any positive number. A value of
     * zero indicates an unlimited number of uploaded files are allowed.
     */
    public static $cardinality = 1;


    /**
     * Be default, all loaders are automaticlly added to the Admin >
     * Tripal > Data Loaders menu.  However, if this loader should be
     * made available via a different menu path, then set it here.  If the
     * value is empty then the path will be the default.
     */
    public static $menu_path = '';

    /**
     * @see TripalImporter::run()
     */
    public function run() {

      // All values provided by the user in the Importer's form widgets are
      // made available to us here by the Class' arguments member variable.
      $arguments = $this->arguments['run_args'];

      // The path to the uploaded file is always made available using the
      // 'files' argument. The importer can support multiple files, therefore
      // this is an array of files, where each has a 'file_path' key specifying
      // where the file is located on the server.
      $file_path = $this->arguments['files'][0]['file_path'];

      // We want to provide a progress report to the end-user so that they:
      // 1) Recognize that the loader is not hung if running a large file, but is
      //    executing
      // 2) Provides some indication for how long the file will take to load.
      //
      // Here we'll get the size of the file and tell the TripalImporter how
      // many "items" we have to process (in this case bytes of the file).
      $filesize = filesize($file_path);
      $this->setTotalItems($filesize);
      $this->setItemsHandled(0);

      // Loop through each line of file.  We use the fgets function so as not
      // to load the entire file into memory but rather to iterate over each
      // line separately.
      $bytes_read = 0;
      $in_fh = fopen($file_path, "r");
      $header = fgets($in_fh);
      $line_num = 1;
      while (($line = fgets($in_fh)) !== FALSE) {
        $line_num = $line_num + 1;

        // Calculate how many bytes we have read from the file and let the
        // importer know how many have been processed so it can provide a
        // progress indicator.
        $bytes_read += drupal_strlen($line);
        $this->setItemsHandled($bytes_read);

        // Remove any trailing white-space from the line.
        $line = trim($line);

        // Split line on a tab into an array.
        $cols = explode("\t", $line);
        $data = [
          'genus' => $cols[0],
          'species' => $cols[1],
          'name' => $cols[2],
          'uniquename' => $cols[3],
          'db_name' => $cols[4],
          'dbxref_accession' => $cols[5],
        ];

        // We want to provide the user with helpful information if their file
        // is not compliant. As such, we thoroughly check each data line
        // before loading it and use the logger to report any concerns.
        $successful = $this->checkData($line_num, $data);

        // During checking of the data we also selected the existing germplasm
        // and datbase. As such, we can just add the new accession here.
        if ($successful) {
          chado_associate_dbxref(
            'stock',
            $data['germplasm_id'],
            [
              'db_id' => $data['db_id'],
              'accession' => $data['dbxref_accession'],
            ]
          );
        }
      }
    }

    /**
     * We want to provide the user with helpful information if their file
     * is not compliant. As such, we thoroughly check each data line
     * before loading it and use the logger to report any concerns.
     *
     * @param int $line_num
     *   The current line number in the file.
     * @param array $data
     *   The data to check. Keys include:
     *    - genus: The genus the existing germplasm is associated with.
     *    - species: The species the existing germplasm is associated with.
     *    - name: The name of the existing germplasm.
     *    - uniquename: The unique identifier of the existing germplasm.
     *    - db_name: the name of the database the new accession is from.
     *    - db_accession: The accession, synonym or DOI to be added.
     * @return array
     *   The same data array from before with the following keys added:
     *    - db_id: the unique identifier of the database
     *    - germplasm_id: the unique identifier of the existing germplasm.
     */
    public function checkData($line_num, &$data) {
      $successful = TRUE;

      if (empty($data['genus'])) {
        $this->logMessage('Line #@n) Missing Genus: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }
      if (empty($data['species'])) {
        $this->logMessage('Line #@n) Missing Species: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }
      if (empty($data['name'])) {
        $this->logMessage('Line #@n) Missing Germplasm Name: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }
      if (empty($data['uniquename'])) {
        $this->logMessage('Line #@n) Missing Germplasm Identifier: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }
      if (empty($data['db_name'])) {
        $this->logMessage('Line #@n) Missing Database Name: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }
      if (empty($data['dbxref_accession'])) {
        $this->logMessage('Line #@n) Missing Database Accession: this field is required',
        ['@n' => $line_num]);
        $successful = FALSE;
      }

      if (!empty($data['db_name'])) {
        $db = chado_get_db(['name' => $data['db_name']]);
        if (empty($db)) {
          $this->logMessage('Line #@n) Unable to find the database named  "@name". Please make sure to create it and then re-import this file.',
          ['@name' => $data['db_name'], '@n' => $line_num], TRIPAL_ERROR);
          $successful = FALSE;
        }
        else {
          $data['db_id'] = $db->db_id;
        }
      }

      $data['germplasm_id'] = chado_query('SELECT stock_id
        FROM {stock} s
        LEFT JOIN {organism} o ON o.organism_id=s.organism_id
        WHERE o.genus=:genus AND o.species=:species
          AND s.name=:name AND s.uniquename=:uname',
        [
          ':genus' => $data['genus'],
          ':species' => $data['species'],
          ':name' => $data['name'],
          ':uname' => $data['uniquename'],
        ])->fetchField();
      if (empty($data['germplasm_id'])) {
        $this->logMessage('Line #@n) Unable to find the germplasm named  "@name" ("@uname") in @genus @species. You can use the "Germplasm Accession" importer to pre-load your germplasm.',
        ['@name' => $data['name'], "@uname" => $data['uniquename'],
          '@genus' => $data['genus'], '@species' => $data['species'],
          '@n' => $line_num],
        TRIPAL_ERROR);
        $successful = FALSE;
      }

      return $successful;
    }
}

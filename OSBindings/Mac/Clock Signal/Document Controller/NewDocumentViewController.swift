//
//  NewDocumentViewController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class NewDocumentCollectionViewItem: NSCollectionViewItem {

	@IBOutlet var backgroundImageView: NSImageView!

}

class NewDocumentViewController: NSViewController {

	@IBOutlet var collectionView : NSCollectionView!
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.
    }
}

//
//  ParsedUser.swift
//  What a Mesh!
//
//  Created by Gabriele Giuli on 2020-02-08.
//  Copyright © 2020 GabrieleGiuli. All rights reserved.
//

import Foundation

class ParsedUser {
    var name: String
    var ID: String
    var messages: [String] = []
    
    init(name: String, ID: String) {
        self.name = name
        self.ID = ID
    }
}
